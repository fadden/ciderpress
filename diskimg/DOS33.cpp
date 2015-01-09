/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Implementation of DiskFSDOS33 and A2FileDOS classes.
 *
 * Works for DOS 3.2 and "wide DOS" as well.
 *
 * BUG: does not keep VolumeUsage up to date.
 */
#include "StdAfx.h"
#include "DiskImgPriv.h"


/*
 * ===========================================================================
 *      DiskFSDOS33
 * ===========================================================================
 */

const int kMaxSectors = 32;

const int kSctSize = 256;
/* do we need a way to override these? */
const int kVTOCTrack = 17;
const int kVTOCSector = 0;
const int kCatalogEntryOffset = 0x0b;   // first entry in cat sect starts here
const int kCatalogEntrySize = 0x23;     // length in bytes of catalog entries
const int kCatalogEntriesPerSect = 7;   // #of entries per catalog sector
const int kEntryDeleted = 0xff;     // this is used for track# of deleted files
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
 * Test this image for DOS3.3-ness.
 *
 * Some notes on tricky disks...
 *
 * DISK019B (Ultima II player master) has a copy of the VTOC in track 11
 * sector 1, which causes a loop back to track 11 sector f.  We may want
 * to be clever here and allow it, but we have to be careful because
 * we must be similarly clever in the VTOC read routines.  (Need a more
 * sophisticated loop detector, since a loop will crank our "foundGood" up.)
 *
 * DISK038B (Congo Bongo) has some "crack" titles and a valid VTOC, but not
 * much else.  Could allow it if the user explicitly told us to use DOS33,
 * but it's a little thin.
 *
 * DISK112B.X (Ultima I player master) has a catalog that jumps around a lot.
 * It's perfectly valid, but we don't really detect it properly.  Forcing
 * DOS interpretation should be acceptable.
 *
 * DISK175A (Standing Stones) has an extremely short but valid catalog track.
 *
 * DISK198B (Aliens+docs) gets 3 and bails with a self-reference.
 */
static DIError TestImage(DiskImg* pImg, DiskImg::SectorOrder imageOrder,
    int* pGoodCount)
{
    DIError dierr = kDIErrNone;
    uint8_t sctBuf[kSctSize];
    int numTracks, numSectors;
    int catTrack, catSect;
    int foundGood = 0;
    int iterations = 0;

    *pGoodCount = 0;

    dierr = pImg->ReadTrackSectorSwapped(kVTOCTrack, kVTOCSector,
                sctBuf, imageOrder, DiskImg::kSectorOrderDOS);
    if (dierr != kDIErrNone)
        goto bail;

    catTrack = sctBuf[0x01];
    catSect = sctBuf[0x02];
    numTracks = sctBuf[0x34];
    numSectors = sctBuf[0x35];

    if (!(sctBuf[0x27] == kMaxTSPairs) ||
        /*!(sctBuf[0x36] == 0 && sctBuf[0x37] == 1) ||*/    // bytes per sect
        !(numTracks <= DiskFSDOS33::kMaxTracks) ||
        !(numSectors == 13 || numSectors == 16 || numSectors == 32) ||
        !(catTrack < numTracks && catSect < numSectors) ||
        0)
    {
        LOGI("  DOS header test failed (order=%d)", imageOrder);
        dierr = kDIErrFilesystemNotFound;
        goto bail;
    }

    foundGood++;        // score one for a valid-looking VTOC

    /*
     * Walk through the catalog track to try to figure out ordering.
     */
    while (catTrack != 0 && catSect != 0 &&
        iterations < DiskFSDOS33::kMaxCatalogSectors)
    {
        dierr = pImg->ReadTrackSectorSwapped(catTrack, catSect, sctBuf,
                    imageOrder, DiskImg::kSectorOrderDOS);
        if (dierr != kDIErrNone) {
            dierr = kDIErrNone;
            break;      /* allow it if earlier stuff was okay */
        }

        if (catTrack == sctBuf[1] && catSect == sctBuf[2] +1)
            foundGood++;
        else if (catTrack == sctBuf[1] && catSect == sctBuf[2]) {
            LOGI(" DOS detected self-reference on cat (%d,%d)",
                catTrack, catSect);
            break;
        }
        catTrack = sctBuf[1];
        catSect = sctBuf[2];
        iterations++;       // watch for infinite loops
    }
    if (iterations >= DiskFSDOS33::kMaxCatalogSectors) {
        /* possible cause: LF->CR conversion screws up link to sector $0a */
        dierr = kDIErrDirectoryLoop;
        LOGI("  DOS directory links cause a loop (order=%d)", imageOrder);
        goto bail;
    }

    LOGI(" DOS    foundGood=%d order=%d", foundGood, imageOrder);
    *pGoodCount = foundGood;

bail:
    return dierr;
}

/*
 * Test to see if the image is a DOS 3.2 or DOS 3.3 disk.
 */
/*static*/ DIError DiskFSDOS33::TestFS(DiskImg* pImg, DiskImg::SectorOrder* pOrder,
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

    if (bestCount >= 4 ||
        (leniency == kLeniencyVery && bestCount >= 2))
    {
        LOGI(" DOS test: bestCount=%d for order=%d", bestCount, bestOrder);
        assert(bestOrder != DiskImg::kSectorOrderUnknown);
        *pOrder = bestOrder;
        *pFormat = DiskImg::kFormatDOS33;
        if (pImg->GetNumSectPerTrack() == 13)
            *pFormat = DiskImg::kFormatDOS32;
        return kDIErrNone;
    }

    LOGI(" DOS33 didn't find valid DOS3.2 or DOS3.3");
    return kDIErrFilesystemNotFound;
}


/*
 * Get things rolling.
 *
 * Since we're assured that this is a valid disk, errors encountered from here
 * on out must be handled somehow, possibly by claiming that the disk is
 * completely full and has no files on it.
 */
DIError DiskFSDOS33::Initialize(InitMode initMode)
{
    DIError dierr = kDIErrNone;

    fVolumeUsage.Create(fpImg->GetNumTracks(), fpImg->GetNumSectPerTrack());

    dierr = ReadVTOC();
    if (dierr != kDIErrNone)
        goto bail;
    //DumpVTOC();

    dierr = ScanVolBitmap();
    if (dierr != kDIErrNone)
        goto bail;

    if (initMode == kInitHeaderOnly) {
        LOGI(" DOS - headerOnly set, skipping file load");
        goto bail;
    }

    /* read the contents of the catalog, creating our A2File list */
    dierr = ReadCatalog();
    if (dierr != kDIErrNone)
        goto bail;

    /* run through and get file lengths and data offsets */
    dierr = GetFileLengths();
    if (dierr != kDIErrNone)
        goto bail;

    /* mark DOS tracks appropriately */
    FixVolumeUsageMap();

    fDiskIsGood = CheckDiskIsGood();

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
 * Read some fields from the disk Volume Table of Contents.
 */
DIError DiskFSDOS33::ReadVTOC(void)
{
    DIError dierr;

    dierr = LoadVolBitmap();
    if (dierr != kDIErrNone)
        goto bail;

    fFirstCatTrack = fVTOC[0x01];
    fFirstCatSector = fVTOC[0x02];
    fVTOCVolumeNumber = fVTOC[0x06];
    fVTOCNumTracks = fVTOC[0x34];
    fVTOCNumSectors = fVTOC[0x35];

    if (fFirstCatTrack >= fpImg->GetNumTracks())
        return kDIErrBadDiskImage;
    if (fFirstCatSector >= fpImg->GetNumSectPerTrack())
        return kDIErrBadDiskImage;

    if (fVTOCNumTracks != fpImg->GetNumTracks()) {
        LOGI(" DOS33 warning: VTOC numtracks %d vs %ld",
            fVTOCNumTracks, fpImg->GetNumTracks());
    }
    if (fVTOCNumSectors != fpImg->GetNumSectPerTrack()) {
        LOGI(" DOS33 warning: VTOC numsect %d vs %d",
            fVTOCNumSectors, fpImg->GetNumSectPerTrack());
    }

    // call SetDiskVolumeNum with the appropriate thing
    UpdateVolumeNum();

bail:
    FreeVolBitmap();
    return dierr;
}

/*
 * Call this if fpImg's volume num (derived from nibble formats) or
 * the VTOC's volume number changes.
 */
void DiskFSDOS33::UpdateVolumeNum(void)
{
    /* use the sector-embedded volume number, if available */
    if (fpImg->GetDOSVolumeNum() == DiskImg::kVolumeNumNotSet)
        SetDiskVolumeNum(fVTOCVolumeNumber);
    else
        SetDiskVolumeNum(fpImg->GetDOSVolumeNum());
    if (fDiskVolumeNum != fVTOCVolumeNumber) {
        LOGI("  NOTE: ignoring VTOC vol (%d) in favor of embedded (%d)",
            fVTOCVolumeNumber, fDiskVolumeNum);
    }
}

/*
 * Set the disk volume number (fDiskVolumeNum) and derived fields.
 */
void DiskFSDOS33::SetDiskVolumeNum(int val)
{
    if (val < 0 || val > 255) {
        // Actual valid range should be 1-254, but it's possible for a
        // sector edit to put invalid stuff here.  It's just one byte
        // though, so 0-255 should be guaranteed.
        assert(false);
        return;
    }
    fDiskVolumeNum = val;
    sprintf(fDiskVolumeName, "DOS%03d", fDiskVolumeNum);
    if (fpImg->GetFSFormat() == DiskImg::kFormatDOS32)
        sprintf(fDiskVolumeID, "DOS 3.2 Volume %03d", fDiskVolumeNum);
    else
        sprintf(fDiskVolumeID, "DOS 3.3 Volume %03d", fDiskVolumeNum);
}


/*
 * Dump some VTOC fields.
 */
void DiskFSDOS33::DumpVTOC(void)
{

    LOGI("VTOC catalog: track=%d sector=%d",
        fFirstCatTrack, fFirstCatSector);
    LOGI("  volnum=%d numTracks=%d numSects=%d",
        fVTOCVolumeNumber, fVTOCNumTracks, fVTOCNumSectors);
}

/*
 * Update an entry in the VolumeUsage map, watching for conflicts.
 */
void DiskFSDOS33::SetSectorUsage(long track, long sector,
    VolumeUsage::ChunkPurpose purpose)
{
    VolumeUsage::ChunkState cstate;

    //LOGI(" DOS setting usage %d,%d to %d", track, sector, purpose);

    fVolumeUsage.GetChunkState(track, sector, &cstate);
    if (cstate.isUsed) {
        cstate.purpose = VolumeUsage::kChunkPurposeConflict;
//      LOGI(" DOS conflicting uses for t=%d s=%d", track, sector);
    } else {
        cstate.isUsed = true;
        cstate.purpose = purpose;
    }
    fVolumeUsage.SetChunkState(track, sector, &cstate);
}

/*
 * Examine the volume bitmap, setting fields in the VolumeUsage map
 * as appropriate.  We mark "isMarkedUsed", but leave "isUsed" clear.  The
 * "isUsed" flag gets set by the DOS catalog track processor and the file
 * scanners.
 *
 * We can't mark the DOS tracks, because there's no reliable way to tell by
 * looking at a DOS disk whether it has a bootable DOS image.  It's possible
 * the tracks are marked in-use because files are stored there.  Some
 * tweaked versions of DOS freed up a few sectors on track 2, so partial
 * allocation isn't a good indicator.
 *
 * What we have to do is wait until we have all the information for the
 * various files, and mark the tracks as owned by DOS if nobody else
 * claims them.
 */
DIError DiskFSDOS33::ScanVolBitmap(void)
{
    DIError dierr;
    VolumeUsage::ChunkState cstate;
    char freemap[32+1] = "--------------------------------";

    cstate.isUsed = false;
    cstate.isMarkedUsed = true;
    cstate.purpose = (VolumeUsage::ChunkPurpose) 0;

    dierr = LoadVolBitmap();
    if (dierr != kDIErrNone)
        goto bail;

    LOGI("  map 0123456789abcdef");

    for (int i = 0; i < kMaxTracks; i++) {
        uint32_t val, origVal;
        int bit;

        val = (uint32_t) fVTOC[0x38 + i*4] << 24;
        val |= (uint32_t) fVTOC[0x39 + i*4] << 16;
        val |= (uint32_t) fVTOC[0x3a + i*4] << 8;
        val |= (uint32_t) fVTOC[0x3b + i*4];
        origVal = val;

        /* init the VolumeUsage stuff */
        for (bit = fpImg->GetNumSectPerTrack()-1; bit >= 0; bit--) {
            freemap[bit] = val & 0x80000000 ? '.' : 'X';

            if (i < fpImg->GetNumTracks() && !(val & 0x80000000)) {
                /* mark the sector as in-use */
                if (fVolumeUsage.SetChunkState(i, bit, &cstate) != kDIErrNone) {
                    assert(false);
                }
            }
            val <<= 1;
        }
        LOGI("  %2d: %s (0x%08x)", i, freemap, origVal);
    }

    /* we know the VTOC is used, so mark it now */
    SetSectorUsage(kVTOCTrack, kVTOCSector, VolumeUsage::kChunkPurposeVolumeDir);

bail:
    FreeVolBitmap();
    return dierr;
}


/*
 * Load the VTOC into the buffer.
 */
DIError DiskFSDOS33::LoadVolBitmap(void)
{
    DIError dierr;

    assert(!fVTOCLoaded);

    dierr = fpImg->ReadTrackSector(kVTOCTrack, kVTOCSector, fVTOC);
    if (dierr != kDIErrNone)
        return dierr;

    fVTOCLoaded = true;
    return kDIErrNone;
}

/*
 * Save our copy of the volume bitmap.
 */
DIError DiskFSDOS33::SaveVolBitmap(void)
{
    if (!fVTOCLoaded) {
        assert(false);
        return kDIErrNotReady;
    }

    return fpImg->WriteTrackSector(kVTOCTrack, kVTOCSector, fVTOC);
}

/*
 * Throw away the volume bitmap, discarding any unsaved changes.
 *
 * It's okay to call this if the bitmap isn't loaded.
 */
void DiskFSDOS33::FreeVolBitmap(void)
{
    fVTOCLoaded = false;

#ifdef _DEBUG
    memset(fVTOC, 0x99, sizeof(fVTOC));
#endif
}

/*
 * Return entry N from the VTOC.
 */
inline uint32_t DiskFSDOS33::GetVTOCEntry(const uint8_t* pVTOC, long track) const
{
    uint32_t val;
    val = (uint32_t) pVTOC[0x38 + track*4] << 24;
    val |= (uint32_t) pVTOC[0x39 + track*4] << 16;
    val |= (uint32_t) pVTOC[0x3a + track*4] << 8;
    val |= (uint32_t) pVTOC[0x3b + track*4];

    return val;
}

/*
 * Allocate a new sector from the unused pool.
 *
 * Only touches the in-memory copy.
 */
DIError DiskFSDOS33::AllocSector(TrackSector* pTS)
{
    uint32_t val;
    uint32_t mask;
    long track, numSectPerTrack;

    /* we could compute "mask", but it's faster and easier to do this */
    numSectPerTrack = GetDiskImg()->GetNumSectPerTrack();
    if (numSectPerTrack == 13)
        mask = 0xfff80000;
    else if (numSectPerTrack == 16)
        mask = 0xffff0000;
    else if (numSectPerTrack == 32)
        mask = 0xffffffff;
    else {
        assert(false);
        return kDIErrInternal;
    }

    /*
     * Start by finding a track with a free sector.  We know it's free
     * because the bits aren't all zero.
     *
     * In theory we don't need "mask", because the DOS format routine is
     * good about leaving the unused bits clear, and nobody else disturbs
     * them.  However, it's best not to rely on it.
     */
    for (track = kVTOCTrack; track > 0; track--) {
        val = GetVTOCEntry(fVTOC, track);
        if ((val & mask) != 0)
            break;
    }
    if (track == 0) {
        long numTracks = GetDiskImg()->GetNumTracks();
        for (track = kVTOCTrack; track < numTracks; track++)
        {
            val = GetVTOCEntry(fVTOC, track);
            if ((val & mask) != 0)
                break;
        }
        if (track == numTracks) {
            LOGI("DOS33 AllocSector unable to find empty sector");
            return kDIErrDiskFull;
        }
    }

    /*
     * We've got the track.  Now find the first free sector.
     */
    int sector;
    sector = numSectPerTrack-1;
    while (sector >= 0) {
        if (val & 0x80000000) {
            //LOGI("+++ allocating T=%d S=%d", track, sector);
            SetSectorUseEntry(track, sector, true);
            break;
        }

        val <<= 1;
        sector--;
    }
    if (sector < 0) {
        assert(false);
        return kDIErrInternal;  // should not have failed
    }

    /*
     * Mostly for fun, update the VTOC allocation thingy.
     */
    fVTOC[0x30] = (uint8_t) track; // last track where alloc happened
    if (track < kVTOCTrack)
        fVTOC[0x31] = 0xff;     // descending
    else
        fVTOC[0x31] = 0x01;     // ascending

    pTS->track = (char) track;
    pTS->sector = (char) sector;

    return kDIErrNone;
}

/*
 * Create an in-use map for an empty disk.  Sets up the VTOC map only.
 *
 * If "withDOS" is set, mark the first 3 tracks as in-use.
 */
DIError DiskFSDOS33::CreateEmptyBlockMap(bool withDOS)
{
    DIError dierr;
    long track, sector, maxTrack;

    dierr = LoadVolBitmap();
    if (dierr != kDIErrNone)
        return dierr;

    if (withDOS)
        maxTrack = 3;
    else
        maxTrack = 1;

    /*
     * Set each bit individually.  Slower, but exercises standard functions.
     *
     * Clear all "in use" flags, except for track 0, track 17, and (if
     * withDOS is set) tracks 1 and 2.
     */
    for (track = fpImg->GetNumTracks()-1; track >= 0; track--) {
        for (sector = fpImg->GetNumSectPerTrack()-1; sector >= 0; sector--) {
            if (track < maxTrack || track == kVTOCTrack)
                SetSectorUseEntry(track, sector, true);
            else
                SetSectorUseEntry(track, sector, false);
        }
    }

    dierr = SaveVolBitmap();
    FreeVolBitmap();
    if (dierr != kDIErrNone)
        return dierr;

    return kDIErrNone;
}

/*
 * Get the state of an entry in the VTOC sector use map.
 *
 * Returns "true" if it's in use, "false" otherwise.
 */
bool DiskFSDOS33::GetSectorUseEntry(long track, int sector) const
{
    assert(fVTOCLoaded);
    assert(track >= 0 && track < fpImg->GetNumTracks());
    assert(sector >= 0 && sector < fpImg->GetNumSectPerTrack());

    uint32_t val, mask;

    val = GetVTOCEntry(fVTOC, track);
    //val = (uint32_t) fVTOC[0x38 + track*4] << 24;
    //val |= (uint32_t) fVTOC[0x39 + track*4] << 16;
    //val |= (uint32_t) fVTOC[0x3a + track*4] << 8;
    //val |= (uint32_t) fVTOC[0x3b + track*4];

    /*
     * The highest-numbered sector is now in the high bit.  If this is a
     * 16-sector disk, the high bit holds the state of sector 15.
     *
     * A '1' indicates the sector is free, '0' indicates it's in use.
     */
    mask = 1L << (32 - fpImg->GetNumSectPerTrack() + sector);
    return (val & mask) == 0;
}

/*
 * Change the state of an entry in the VTOC sector use map.
 */
void DiskFSDOS33::SetSectorUseEntry(long track, int sector, bool inUse)
{
    assert(fVTOCLoaded);
    assert(track >= 0 && track < fpImg->GetNumTracks());
    assert(sector >= 0 && sector < fpImg->GetNumSectPerTrack());

    uint32_t val, mask;

    val = GetVTOCEntry(fVTOC, track);

    /* highest sector is always in the high bit */
    mask = 1L << (32 - fpImg->GetNumSectPerTrack() + sector);
    if (inUse)
        val &= ~mask;
    else
        val |= mask;

    fVTOC[0x38 + track*4] = (uint8_t) (val >> 24);
    fVTOC[0x39 + track*4] = (uint8_t) (val >> 16);
    fVTOC[0x3a + track*4] = (uint8_t) (val >> 8);
    fVTOC[0x3b + track*4] = (uint8_t) val;
}


/*
 * Get the amount of free space remaining.
 */
DIError DiskFSDOS33::GetFreeSpaceCount(long* pTotalUnits, long* pFreeUnits,
    int* pUnitSize) const
{
    DIError dierr;
    long track, sector, freeSectors;

    dierr = const_cast<DiskFSDOS33*>(this)->LoadVolBitmap();
    if (dierr != kDIErrNone)
        return dierr;

    freeSectors = 0;
    for (track = GetDiskImg()->GetNumTracks()-1; track >= 0; track--) {
        for (sector = GetDiskImg()->GetNumSectPerTrack()-1; sector >= 0; sector--)
        {
            if (!GetSectorUseEntry(track, sector))
                freeSectors++;
        }
    }

    *pTotalUnits = fpImg->GetNumTracks() * fpImg->GetNumSectPerTrack();
    *pFreeUnits = freeSectors;
    *pUnitSize = kSectorSize;

    const_cast<DiskFSDOS33*>(this)->FreeVolBitmap();
    return kDIErrNone;
}


/*
 * Fix up the DOS tracks.
 *
 * Any sectors marked used but not actually in use by a file are marked as
 * in use by the system.  We have to be somewhat careful here because some
 * disks had DOS removed to add space, un-set the last few sectors of track 2
 * that weren't actually used by DOS, or did some other funky thing.
 */
void DiskFSDOS33::FixVolumeUsageMap(void)
{
    VolumeUsage::ChunkState cstate;
    int track, sector;

    for (track = 0; track < 3; track++) {
        for (sector = 0; sector < fpImg->GetNumSectPerTrack(); sector++) {
            fVolumeUsage.GetChunkState(track, sector, &cstate);
            if (cstate.isMarkedUsed && !cstate.isUsed) {
                cstate.isUsed = true;
                cstate.purpose = VolumeUsage::kChunkPurposeSystem;
                fVolumeUsage.SetChunkState(track, sector, &cstate);
            }
        }
    }
}


/*
 * Read the disk's catalog.
 *
 * NOTE: supposedly DOS stops reading the catalog track when it finds the
 * first entry with a 00 byte, which is why deleted files use ff.  If so,
 * it *might* make sense to mimic this behavior, though on a health disk
 * we shouldn't be finding garbage anyway.
 *
 * Fills out "fCatalogSectors" as it works.
 */
DIError DiskFSDOS33::ReadCatalog(void)
{
    DIError dierr = kDIErrNone;
    uint8_t sctBuf[kSctSize];
    int catTrack, catSect;
    int iterations;

    catTrack = fFirstCatTrack;
    catSect = fFirstCatSector;
    iterations = 0;

    memset(fCatalogSectors, 0, sizeof(fCatalogSectors));

    while (catTrack != 0 && catSect != 0 && iterations < kMaxCatalogSectors)
    {
        SetSectorUsage(catTrack, catSect, VolumeUsage::kChunkPurposeVolumeDir);

        LOGI(" DOS33 reading catalog sector T=%d S=%d", catTrack, catSect);
        dierr = fpImg->ReadTrackSector(catTrack, catSect, sctBuf);
        if (dierr != kDIErrNone)
            goto bail;

        /*
         * Watch for flaws that the DOS detector allows.
         */
        if (catTrack == sctBuf[0x01] && catSect == sctBuf[0x02]) {
            LOGI(" DOS detected self-reference on cat (%d,%d)",
                catTrack, catSect);
            break;
        }

        /*
         * Check the next track/sector in the chain.  If the pointer is
         * broken, there's a very good chance that this isn't really a
         * catalog sector, so we want to bail out now.
         */
        if (sctBuf[0x01] >= fpImg->GetNumTracks() ||
            sctBuf[0x02] >= fpImg->GetNumSectPerTrack())
        {
            LOGI(" DOS bailing out early on catalog read due to funky T/S");
            break;
        }

        dierr = ProcessCatalogSector(catTrack, catSect, sctBuf);
        if (dierr != kDIErrNone)
            goto bail;

        fCatalogSectors[iterations].track = catTrack;
        fCatalogSectors[iterations].sector = catSect;

        catTrack = sctBuf[0x01];
        catSect = sctBuf[0x02];

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
DIError DiskFSDOS33::ProcessCatalogSector(int catTrack, int catSect,
    const uint8_t* sctBuf)
{
    A2FileDOS* pFile;
    const uint8_t* pEntry;
    int i;

    pEntry = &sctBuf[kCatalogEntryOffset];

    for (i = 0; i < kCatalogEntriesPerSect; i++) {
        if (pEntry[0x00] != kEntryUnused && pEntry[0x00] != kEntryDeleted) {
            pFile = new A2FileDOS(this);

            pFile->SetQuality(A2File::kQualityGood);

            pFile->fTSListTrack = pEntry[0x00];
            pFile->fTSListSector = pEntry[0x01];
            pFile->fLocked = (pEntry[0x02] & 0x80) != 0;
            switch (pEntry[0x02] & 0x7f) {
            case 0x00:  pFile->fFileType = A2FileDOS::kTypeText;        break;
            case 0x01:  pFile->fFileType = A2FileDOS::kTypeInteger;     break;
            case 0x02:  pFile->fFileType = A2FileDOS::kTypeApplesoft;   break;
            case 0x04:  pFile->fFileType = A2FileDOS::kTypeBinary;      break;
            case 0x08:  pFile->fFileType = A2FileDOS::kTypeS;           break;
            case 0x10:  pFile->fFileType = A2FileDOS::kTypeReloc;       break;
            case 0x20:  pFile->fFileType = A2FileDOS::kTypeA;           break;
            case 0x40:  pFile->fFileType = A2FileDOS::kTypeB;           break;
            default:
                /* some odd arrangement of bit flags? */
                LOGI(" DOS33 peculiar filetype byte 0x%02x", pEntry[0x02]);
                pFile->fFileType = A2FileDOS::kTypeUnknown;
                pFile->SetQuality(A2File::kQualitySuspicious);
                break;
            }

            memcpy(pFile->fFileName, &pEntry[0x03], A2FileDOS::kMaxFileName);
            pFile->fFileName[A2FileDOS::kMaxFileName] = '\0';
            pFile->FixFilename();

            pFile->fLengthInSectors = pEntry[0x21];
            pFile->fLengthInSectors |= (uint16_t) pEntry[0x22] << 8;

            pFile->fCatTS.track = catTrack;
            pFile->fCatTS.sector = catSect;
            pFile->fCatEntryNum = i;

            /* can't do these yet, so just set to defaults */
            pFile->fLength = 0;
            pFile->fSparseLength = 0;
            pFile->fDataOffset = 0;

            AddFileToList(pFile);
        }

        pEntry += kCatalogEntrySize;
    }

    return kDIErrNone;
}


/*
 * Perform consistency checks on the filesystem.
 *
 * Returns "true" if disk appears to be perfect, "false" otherwise.
 */
bool DiskFSDOS33::CheckDiskIsGood(void)
{
    DIError dierr;
    const DiskImg* pDiskImg = GetDiskImg();
    bool result = true;
    int i;

    dierr = LoadVolBitmap();
    if (dierr != kDIErrNone)
        goto bail;

    /*
     * Make sure the VTOC is marked in use, or things could go badly.
     * Ditto for the catalog tracks.
     */
    if (!GetSectorUseEntry(kVTOCTrack, kVTOCSector)) {
        fpImg->AddNote(DiskImg::kNoteWarning, "VTOC sector marked as free.");
        result = false;
    }
    for (i = 0; i < kMaxCatalogSectors; i++) {
        if (!GetSectorUseEntry(fCatalogSectors[i].track,
            fCatalogSectors[i].sector))
        {
            fpImg->AddNote(DiskImg::kNoteWarning,
                "Catalog sector %d,%d is marked as free.",
                fCatalogSectors[i].track, fCatalogSectors[i].sector);
            result = false;
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
    long track, sector;
    long notMarked, extraUsed, conflicts;
    notMarked = extraUsed = conflicts = 0;
    for (track = 0; track < pDiskImg->GetNumTracks(); track++) {
        for (sector = 0; sector < pDiskImg->GetNumSectPerTrack(); sector++) {
            dierr = fVolumeUsage.GetChunkState(track, sector, &cstate);
            if (dierr != kDIErrNone) {
                fpImg->AddNote(DiskImg::kNoteWarning,
                    "Internal volume usage error on t=%ld s=%ld.",
                    track, sector);
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
    }
    if (extraUsed > 0) {
        fpImg->AddNote(DiskImg::kNoteInfo,
            "%ld sector%s marked used but not part of any file.",
            extraUsed, extraUsed == 1 ? " is" : "s are");
        // not a problem, really
    }
    if (notMarked > 0) {
        fpImg->AddNote(DiskImg::kNoteWarning,
            "%ld sector%s used by files but not marked used.",
            notMarked, notMarked == 1 ? " is" : "s are");
        result = false;
    }
    if (conflicts > 0) {
        fpImg->AddNote(DiskImg::kNoteWarning,
            "%ld sector%s used by more than one file.",
            conflicts, conflicts == 1 ? " is" : "s are");
        result = false;
    }

    /*
     * Scan for "damaged" files or "suspicious" files diagnosed earlier.
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
 * Run through our list of files, computing the lengths and marking file
 * usage in the VolumeUsage object.
 */
DIError DiskFSDOS33::GetFileLengths(void)
{
    A2FileDOS* pFile;
    TrackSector* tsList = NULL;
    TrackSector* indexList = NULL;
    int tsCount;
    int indexCount;

    pFile = (A2FileDOS*) GetNextFile(NULL);
    while (pFile != NULL) {
        DIError dierr;
        dierr = pFile->LoadTSList(&tsList, &tsCount, &indexList, &indexCount);
        if (dierr != kDIErrNone) {
            LOGI("DOS failed loading TS list for '%s'",
                pFile->GetPathName());
            pFile->SetQuality(A2File::kQualityDamaged);
        } else {
            MarkFileUsage(pFile, tsList, tsCount, indexList, indexCount);
            dierr = ComputeLength(pFile, tsList, tsCount);
            if (dierr != kDIErrNone) {
                LOGI("DOS unable to get length for '%s'",
                    pFile->GetPathName());
                pFile->SetQuality(A2File::kQualityDamaged);
            }
        }

        if (pFile->fLengthInSectors != indexCount + tsCount) {
            LOGI("DOS NOTE: file '%s' has len-in-sect=%d but actual=%d",
                pFile->GetPathName(), pFile->fLengthInSectors,
                indexCount + tsCount);
            // expected on sparse random-access text files
        }

        delete[] tsList;
        delete[] indexList;
        tsList = indexList = NULL;

        pFile = (A2FileDOS*) GetNextFile(pFile);
    }

    return kDIErrNone;
}

/*
 * Compute the length and starting data offset of the file.
 *
 * For Text, there are two situations: sequential and random.  For
 * sequential text files, we just need to find the first 00 byte.  For
 * random, there can be 00s everywhere, and in fact there can be holes
 * in the T/S list.  The plan: since DOS doesn't let you "truncate" a
 * text file, just scan the last sector for 00.  The length is the
 * number of previous T/S entries * 256 plus the sector offset.
 * --> This does the wrong thing for random-access text files, which
 * need to retain their full length, and doesn't work right for sequential
 * text files that (somehow) had their last block over-allocated.  It does
 * the right thing most of the time, but we either need to be more clever
 * here or provide a way to override the default (bool fTrimTextFiles?).
 *
 * For Applesoft and Integer, the file length is stored as the first two
 * bytes of the file.
 *
 * For Binary, the file length is stored in the second two bytes (after
 * the two-byte address).  Some files (with low-memory loaders) used a
 * fake length, and DDD 2.x sets both address and length to zero.
 *
 * For Reloc, S, A2, B2, and "unknown", we just multiply the sector count.
 * We get an accurate sector count from the T/S list (the value in the
 * directory entry might have been tampered with).
 *
 * To handle DDD 2.x files correctly, we need to identify them as such by
 * looking for 'B' with address=0 and length=0, a T/S count of at least 8
 * (the smallest possible compression of a 35-track disk is 2385 bytes),
 * and a '<' in the filename.  If found, we start from offset=0
 * (because DDD Pro 1.x includes the 4 leading bytes) and include all
 * sectors, we'll get the actual file plus at most 256 garbage bytes.
 *
 * On success, we set the following:
 *  pFile->fLength
 *  pFile->fSparseLength
 *  pFile->fDataOffset
 */
DIError DiskFSDOS33::ComputeLength(A2FileDOS* pFile, const TrackSector* tsList,
    int tsCount)
{
    DIError dierr = kDIErrNone;
    uint8_t sctBuf[kSctSize];

    assert(pFile != NULL);
    assert(tsList != NULL);
    assert(tsCount >= 0);

    pFile->fDataOffset = 0;

    pFile->fAuxType = 0;
    if (pFile->fFileType == A2FileDOS::kTypeApplesoft)
        pFile->fAuxType = 0x0801;
    /* for text files it's default record length; assume zero */

    if (tsCount == 0) {
        /* no data at all */
        pFile->fLength = 0;
    } else if (pFile->fFileType == A2FileDOS::kTypeApplesoft ||
        pFile->fFileType == A2FileDOS::kTypeInteger ||
        pFile->fFileType == A2FileDOS::kTypeBinary)
    {
        /* read first sector and analyze it */
        //LOGI(" DOS reading first file sector");
        dierr = fpImg->ReadTrackSector(tsList[0].track, tsList[0].sector,
                    sctBuf);
        if (dierr != kDIErrNone)
            goto bail;

        if (pFile->fFileType == A2FileDOS::kTypeBinary) {
            pFile->fAuxType =
                sctBuf[0x00] | (uint16_t) sctBuf[0x01] << 8;
            pFile->fLength =
                sctBuf[0x02] | (uint16_t) sctBuf[0x03] << 8;
            pFile->fDataOffset = 4; // take the above into account
        } else {
            pFile->fLength =
                sctBuf[0x00] | (uint16_t) sctBuf[0x01] << 8;
            pFile->fDataOffset = 2; // take the above into account
        }

        if (pFile->fFileType == A2FileDOS::kTypeBinary &&
            pFile->fLength == 0 && pFile->fAuxType == 0 &&
            tsCount >= 8 &&
            strchr(pFile->fFileName, '<') != NULL &&
            strchr(pFile->fFileName, '>') != NULL)
        {
            LOGI(" DOS found probable DDD archive, tweaking '%s' (lis=%u)",
                pFile->GetPathName(), pFile->fLengthInSectors);
            //dierr = TrimLastSectorDown(pFile, tsBuf, WrapperDDD::kMaxDDDZeroCount);
            //if (dierr != kDIErrNone)
            //  goto bail;
            //LOGI(" DOS scanned DDD file '%s' to length %ld (tsCount=%d)",
            //  pFile->fFileName, pFile->fLength, pFile->fTSListCount);
            pFile->fLength = tsCount * kSctSize;
            pFile->fDataOffset = 0;
        }

        /* catch bogus lengths in damaged A/I/B files */
        if (pFile->fLength > tsCount * kSctSize) {
            LOGI(" DOS33 capping max len from %ld to %d in '%s'",
                (long) pFile->fLength, tsCount * kSctSize,
                pFile->fFileName);
            pFile->fLength = tsCount * kSctSize - pFile->fDataOffset;
            if (pFile->fLength < 0)     // can't happen here?
                pFile->fLength = 0;

            /*
             * This could cause a problem, because if the user changes a 'T'
             * file to 'B', the bogus file length will mark the file as
             * "suspicious" and we won't allow writing to the disk (which
             * makes it hard to switch the file type back).  We really don't
             * want to weaken this test though.
             */
            pFile->SetQuality(A2File::kQualitySuspicious);
        }

    } else if (pFile->fFileType == A2FileDOS::kTypeText) {
        /* scan text file */
        pFile->fLength = tsCount * kSctSize;
        dierr = TrimLastSectorUp(pFile, tsList[tsCount-1]);
        if (dierr != kDIErrNone)
            goto bail;

        LOGI(" DOS scanned text file '%s' down to %d+%ld = %ld",
            pFile->fFileName,
            (tsCount-1) * kSctSize,
            (long)pFile->fLength - (tsCount-1) * kSctSize,
            (long)pFile->fLength);

        /* TO DO: something clever to discern random access record length? */
    } else {
        /* S/R/A/B: just use the TS count */
        pFile->fLength = tsCount * kSctSize;
    }

    /*
     * Compute the sparse length for random-access text files.
     */
    int i, sparseCount;
    sparseCount = 0;
    for (i = 0; i < tsCount; i++) {
        if (tsList[i].track == 0 && tsList[i].sector == 0)
            sparseCount++;
    }
    pFile->fSparseLength = pFile->fLength - sparseCount * kSctSize;
    if (pFile->fSparseLength == -pFile->fDataOffset) {
        /*
         * This can happen for a completely sparse file.  Looks sort of
         * stupid to have a length of "-4", so force it to zero.
         */
        pFile->fSparseLength = 0;
    }

bail:
    return dierr;
}

/*
 * Trim the zeroes off the end of the last sector.  We begin at the start
 * of the sector and stop at the first zero found.
 *
 * Modifies pFile->fLength, which should be set to a roughly accurate
 * value on entry.
 *
 * The caller should endeavor to strip out T=0 S=0 entries that come after
 * the body of the file.  They're valid in the middle for random-access
 * text files.
 */
DIError DiskFSDOS33::TrimLastSectorUp(A2FileDOS* pFile, TrackSector lastTS)
{
    DIError dierr;
    uint8_t sctBuf[kSctSize];
    int i;

    if (lastTS.track == 0) {
        /* happens on files with lots of "sparse" space at the end */
        return kDIErrNone;
    }

    //LOGI(" DOS reading LAST file sector");
    dierr = fpImg->ReadTrackSector(lastTS.track, lastTS.sector, sctBuf);
    if (dierr != kDIErrNone)
        goto bail;

    /* start with EOF equal to previous sectors */
    pFile->fLength -= kSctSize;
    for (i = 0; i < kSctSize; i++) {
        if (sctBuf[i] == 0x00)
            break;
        else
            pFile->fLength++;
    }

bail:
    return dierr;
}

/*
 * Given lists of tracks and sector for data and TS index sectors, set the
 * entries in the volume usage map.
 */
void DiskFSDOS33::MarkFileUsage(A2FileDOS* pFile, TrackSector* tsList, int tsCount,
    TrackSector* indexList, int indexCount)
{
    int i;

    for (i = 0; i < tsCount; i++) {
        /* mark all sectors as in-use by file */
        if (tsList[i].track == 0 && tsList[i].sector == 0) {
            /* sparse sector in random-access text file */
        } else {
            SetSectorUsage(tsList[i].track, tsList[i].sector,
                VolumeUsage::kChunkPurposeUserData);
        }
    }

    for (i = 0; i < indexCount; i++) {
        /* mark the T/S sectors as in-use by file structures */
        SetSectorUsage(indexList[i].track, indexList[i].sector,
            VolumeUsage::kChunkPurposeFileStruct);
    }
}



#if 0
/*
 * Trim the zeroes off the end of the last sector.  We begin at the end
 * of the sector and back up.
 *
 * It is possible (one out of between 128 and 256 times) that we have just
 * the trailing zero in this sector, and we need to back up to the previous
 * sector to find the actual end.  We know a file can end with three zeroes
 * and we suspect it might be possible to end with four, which means we could
 * have between 0 and 3 zeroes in the previous sector, and between 1 and 4
 * in this sector.  If we just tack on three more zeroes, we weaken our
 * length test slightly, because we must allow a "slop" of up to seven bytes.
 * It's a little more work, but scanning the next-to-last sector is probably
 * worthwhile given the otherwise flaky nature of DDD storage.
 */
DIError
DiskFSDOS33::TrimLastSectorDown(A2FileDOS* pFile, uint16_t* tsBuf,
    int maxZeroCount)
{
    DIError dierr;
    uint8_t sctBuf[kSctSize];
    int i;

    //LOGI(" DOS reading LAST file sector");
    dierr = fpImg->ReadTrackSector(
                pFile->TSTrack(tsBuf[pFile->fTSListCount-1]),
                pFile->TSSector(tsBuf[pFile->fTSListCount-1]),
                sctBuf);
    if (dierr != kDIErrNone)
        goto bail;

    /* find the first trailing zero by finding the last non-zero */
    for (i = kSctSize-1; i >= 0; i--) {
        if (sctBuf[i] != 0x00)
            break;
    }
    if (i < 0) {
        /* sector was nothing but zeroes */
        DebugBreak();
    } else {
        /* peg it at 256; if it went over that, DDD would've added a sector */
        i += maxZeroCount;
        if (i > kSctSize)
            i = kSctSize;
        pFile->fLength = (pFile->fTSListCount-1) * kSctSize + i;
    }

bail:
    return dierr;
}
#endif


/*
 * Convert high ASCII to low ASCII.
 *
 * Some people put inverse and flashing text into filenames, not to mention
 * control characters, so we have to cope with those too.
 *
 * We modify the first "len" bytes of "buf" in place.
 */
/*static*/ void DiskFSDOS33::LowerASCII(uint8_t* buf, long len)
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
 * Determine whether or not "name" is a valid DOS 3.3 filename.
 *
 * Names can be up to 30 characters and can contain absolutely anything.
 * To make life easier on DOS users, we ban the use of the comma, block
 * control characters and high ASCII, and don't allow completely blank
 * names.  Later on we will later convert to upper case, so we allow lower
 * case letters here.
 *
 * Filenames simply pad out to 30 characters with spaces, so the only
 * "invalid" character is a trailing space.  Because we're using C-style
 * strings, we implicitly ban the use of '\0' in the name.
 */
/*static*/ bool DiskFSDOS33::IsValidFileName(const char* name)
{
    bool nonSpace = false;
    int len = 0;

    /* count letters, skipping control chars */
    while (*name != '\0') {
        char ch = *name++;

        if (ch < 0x20 || ch >= 0x7f || ch == ',')
            return false;
        if (ch != 0x20)
            nonSpace = true;
        len++;
    }
    if (len == 0 || len > A2FileDOS::kMaxFileName)
        return false;       // can't be empty, can't be huge
    if (!nonSpace)
        return false;       // must have one non-ctrl non-space char
    if (*(name-1) == ' ')
        return false;       // no trailing spaces

    return true;
}

/*
 * Determine whether "name" is a valid volume number.
 */
/*static*/ bool DiskFSDOS33::IsValidVolumeName(const char* name)
{
    long val;
    char* endp;

    val = strtol(name, &endp, 10);
    if (*endp != '\0' || val < 1 || val > 254)
        return false;

    return true;
}


/*
 * Put a DOS 3.2/3.3 filesystem image on the specified DiskImg.
 *
 * If "volName" is "DOS", a basic DOS image will be written to the first three
 * tracks of the disk, and the in-use map will be updated appropriately.
 *
 * It would seem at first glance that putting the volume number into the
 * volume name string would make the interface more consistent with the
 * rest of the filesystems.  The first glance is substantially correct, but
 * the DOS stuff has a separate "set volume number" interface already, used
 * to deal with the various locations where volume numbers can be stored
 * (2MG header, VTOC, sector address headers) in the various formats.
 *
 * So, instead of stuffing the volume number into "volName" and creating
 * some other path for specifying "add DOS image", I continue to use the
 * defined ways of setting the volume number and abuse "volName" slightly.
 */
DIError DiskFSDOS33::Format(DiskImg* pDiskImg, const char* volName)
{
    DIError dierr = kDIErrNone;
    uint8_t sctBuf[256];
    bool addDOS = false;

    if (pDiskImg->GetNumTracks() < kMinTracks ||
        pDiskImg->GetNumTracks() > kMaxTracks)
    {
        LOGI(" DOS33 can't format numTracks=%ld", pDiskImg->GetNumTracks());
        return kDIErrInvalidArg;
    }
    if (pDiskImg->GetNumSectPerTrack() != 13 &&
        pDiskImg->GetNumSectPerTrack() != 16 &&
        pDiskImg->GetNumSectPerTrack() != 32)
    {
        LOGI(" DOS33 can't format sectors=%d",
            pDiskImg->GetNumSectPerTrack());
        return kDIErrInvalidArg;
    }

    if (volName != NULL && strcmp(volName, "DOS") == 0) {
        if (pDiskImg->GetNumSectPerTrack() != 16 &&
            pDiskImg->GetNumSectPerTrack() != 13)
        {
            LOGI("NOTE: numSectPerTrack = %d, can't write DOS tracks",
                pDiskImg->GetNumSectPerTrack());
            return kDIErrInvalidArg;
        }
        addDOS = true;
    }

    /* set fpImg so calls that rely on it will work; we un-set it later */
    assert(fpImg == NULL);
    SetDiskImg(pDiskImg);

    LOGI(" DOS33 formatting disk image (sectorOrder=%d)",
        fpImg->GetSectorOrder());

    /* write DOS sectors */
    dierr = fpImg->OverrideFormat(fpImg->GetPhysicalFormat(),
                DiskImg::kFormatGenericDOSOrd, fpImg->GetSectorOrder());
    if (dierr != kDIErrNone)
        goto bail;

    /*
     * We should now zero out the disk blocks, but on a 32MB volume that can
     * take a little while.  The blocks are zeroed for us when a disk is
     * created, so this is really only needed if we're re-formatting an
     * existing disk.  CiderPress currently doesn't do that, so we're going
     * to skip it here.
     */
//  dierr = fpImg->ZeroImage();
    LOGI(" DOS33  (not zeroing blocks)");

    if (addDOS) {
        dierr = WriteDOSTracks(pDiskImg->GetNumSectPerTrack());
        if (dierr != kDIErrNone)
            goto bail;
    }

    /*
     * Set up the static fields in the VTOC.
     */
    dierr = LoadVolBitmap();
    if (dierr != kDIErrNone)
        goto bail;
    fVTOC[0x00] = 0x04;                             // (no reason)
    fVTOC[0x01] = kVTOCTrack;                       // first cat track
    fVTOC[0x02] = fpImg->GetNumSectPerTrack()-1;    // first cat sector
    fVTOC[0x03] = 3;                                // version
    if (fpImg->GetDOSVolumeNum() == DiskImg::kVolumeNumNotSet)
        fVTOC[0x06] = kDefaultVolumeNum;            // VTOC volume number
    else
        fVTOC[0x06] = (uint8_t) fpImg->GetDOSVolumeNum();
    fVTOC[0x27] = 122;                              // max T/S pairs
    fVTOC[0x30] = kVTOCTrack+1;                     // last alloc
    fVTOC[0x31] = 1;                                // ascending
    fVTOC[0x34] = (uint8_t)fpImg->GetNumTracks(); // #of tracks
    fVTOC[0x35] = fpImg->GetNumSectPerTrack();      // #of sectors
    fVTOC[0x36] = 0x00;                             // bytes/sector (lo)
    fVTOC[0x37] = 0x01;                             // bytes/sector (hi)
    if (pDiskImg->GetNumSectPerTrack() == 13) {
        // minor changes for DOS 3.2
        fVTOC[0x00] = 0x02;
        fVTOC[0x03] = 2;
    }

    dierr = SaveVolBitmap();
    FreeVolBitmap();
    if (dierr != kDIErrNone)
        goto bail;

    /*
     * Fill the sectors in the catalog track.
     */
    int sect;
    memset(sctBuf, 0, sizeof(sctBuf));
    sctBuf[0x01] = kVTOCTrack;
    for (sect = fpImg->GetNumSectPerTrack()-1; sect > 1; sect--) {
        sctBuf[0x02] = sect-1;

        dierr = fpImg->WriteTrackSector(kVTOCTrack, sect, sctBuf);
        if (dierr != kDIErrNone)
            goto bail;
    }

    /*
     * Generate the initial block usage map.  The only entries in use are
     * right at the start of the disk.
     */
    CreateEmptyBlockMap(addDOS);

    /* check our work, and set some object fields, by reading what we wrote */
    dierr = ReadVTOC();
    if (dierr != kDIErrNone) {
        LOGI(" GLITCH: couldn't read header we just wrote (err=%d)", dierr);
        goto bail;
    }

    /* don't do this -- assume they're going to call Initialize() later */
    //ScanVolBitmap();

bail:
    SetDiskImg(NULL);    // shouldn't really be set by us
    return dierr;
}

/*
 * Write a DOS image into tracks 0-2.
 *
 * This takes the number of sectors per track as an argument so we can figure
 * out which version of DOS to write.  This probably ought to be an enum so
 * we can specify various versions of DOS.
 */
DIError DiskFSDOS33::WriteDOSTracks(int sectPerTrack)
{
    DIError dierr = kDIErrNone;
    long track, sector;
    const uint8_t* buf = gDOS33Tracks;

    if (sectPerTrack == 13) {
        LOGI("  DOS33 writing DOS 3.3 tracks");
        buf = gDOS32Tracks;

        for (track = 0; track < 3; track++) {
            for (sector = 0; sector < 13; sector++) {
                dierr = fpImg->WriteTrackSector(track, sector, buf);
                if (dierr != kDIErrNone)
                    goto bail;
                buf += kSctSize;
            }
        }
    } else if (sectPerTrack == 16) {
        LOGI("  DOS33 writing DOS 3.3 tracks");
        buf = gDOS33Tracks;

        // this should be used for 32-sector disks

        for (track = 0; track < 3; track++) {
            for (sector = 0; sector < 16; sector++) {
                dierr = fpImg->WriteTrackSector(track, sector, buf);
                if (dierr != kDIErrNone)
                    goto bail;
                buf += kSctSize;
            }
        }
    } else {
        LOGI("  DOS33 *not* writing DOS tracks to %d-sector disk",
            sectPerTrack);
        assert(false);
    }

bail:
    return dierr;
}

/*
 * Normalize a DOS 3.3 path.  Used when adding files from DiskArchive.
 * The path may contain subdirectory components, which we need to strip away.
 *
 * "*pNormalizedBufLen" is used to pass in the length of the buffer and
 * pass out the length of the string (should the buffer prove inadequate).
 */
DIError DiskFSDOS33::NormalizePath(const char* path, char fssep,
    char* normalizedBuf, int* pNormalizedBufLen)
{
    DIError dierr = kDIErrNone;
    char tmpBuf[A2FileDOS::kMaxFileName+1];
    int len;

    DoNormalizePath(path, fssep, tmpBuf);
    len = strlen(tmpBuf)+1;

    if (*pNormalizedBufLen < len)
        dierr = kDIErrDataOverrun;
    else
        strcpy(normalizedBuf, tmpBuf);
    *pNormalizedBufLen = len;

    return dierr;
}

/*
 * Normalize a DOS 3.3 pathname.  Lower case becomes upper case, control
 * characters and high ASCII get stripped, and ',' becomes '_'.
 *
 * "outBuf" must be able to hold kMaxFileName+1 characters.
 */
void DiskFSDOS33::DoNormalizePath(const char* name, char fssep, char* outBuf)
{
    char* outp = outBuf;
    const char* cp;

    /* throw out leading pathname, if any */
    if (fssep != '\0') {
        cp = strrchr(name, fssep);
        if (cp != NULL)
            name = cp+1;
    }

    while (*name != '\0' && (outp - outBuf) <= A2FileDOS::kMaxFileName) {
        if (*name >= 0x20 && *name < 0x7f) {
            if (*name == ',')
                *outp = '_';
            else
                *outp = toupper(*name);

            outp++;
        }
        name++;
    }
    *outp = '\0';

    if (*outBuf == '\0') {
        /* nothing left */
        strcpy(outBuf, "BLANK");
    }
}

/*
 * Create a file on a DOS 3.2/3.3 disk.
 *
 * The file will be created with an empty T/S list.
 *
 * It is not possible to set the aux type here.  Aux types only apply to 'B'
 * files, and since they're stored in the first data sector (which we don't
 * create), there's nowhere to put it.  We stuff it into the aux type value
 * in the linear file list, on the assumption that somebody will come along
 * and politely Write to the file, even if it's zero bytes long.
 *
 * (Technically speaking, setting the file type here is bogus, because a
 * 'B' file with no data sectors is invalid.  However, we don't want to
 * handle arbitrary changes later -- switching from 'T' to 'B' requires
 * either rewriting the entire file, or confusing the user by changing the
 * type without adjusting the first 4 bytes -- so we set it now.  It's also
 * helpful to set it now because the Write routine needs to know how many
 * bytes offset from the start of the file it needs to be.  We could avoid
 * most of this weirdness by just going ahead and allocating the first
 * sector of the file now, and modifying the Write() function to understand
 * that the first block is already there.  Need to do that someday.)
 */
DIError DiskFSDOS33::CreateFile(const CreateParms* pParms,  A2File** ppNewFile)
{
    DIError dierr = kDIErrNone;
    const bool createUnique = (GetParameter(kParm_CreateUnique) != 0);
    char normalName[A2FileDOS::kMaxFileName+1];
//  char storageName[A2FileDOS::kMaxFileName+1];
    A2FileDOS::FileType fileType;
    A2FileDOS* pNewFile = NULL;

    if (fpImg->GetReadOnly())
        return kDIErrAccessDenied;
    if (!fDiskIsGood)
        return kDIErrBadDiskImage;

    assert(pParms != NULL);
    assert(pParms->pathName != NULL);
    assert(pParms->storageType == A2FileProDOS::kStorageSeedling);
    LOGI(" DOS33 ---v--- CreateFile '%s'", pParms->pathName);

    *ppNewFile = NULL;

    DoNormalizePath(pParms->pathName, pParms->fssep, normalName);

    /*
     * See if the file already exists.
     *
     * If "create unique" is set, we append digits until the name doesn't
     * match any others.  The name will be modified in place.
     */
    if (createUnique) {
        MakeFileNameUnique(normalName);
    } else {
        if (GetFileByName(normalName) != NULL) {
            LOGI(" DOS33 create: normalized name '%s' already exists",
                normalName);
            dierr = kDIErrFileExists;
            goto bail;
        }
    }

    fileType = A2FileDOS::ConvertFileType(pParms->fileType, 0);

    /*
     * Allocate a directory entry and T/S list.
     */
    uint8_t sctBuf[kSctSize];
    TrackSector catSect;
    TrackSector tsSect;
    int catEntry;
    A2FileDOS* pPrevEntry;

    dierr = LoadVolBitmap();
    if (dierr != kDIErrNone)
        goto bail;

    /* allocate a sector for the T/S list, and zero it out */
    dierr = AllocSector(&tsSect);
    if (dierr != kDIErrNone)
        goto bail;

    memset(sctBuf, 0, kSctSize);
    dierr = fpImg->WriteTrackSector(tsSect.track, tsSect.sector, sctBuf);
    if (dierr != kDIErrNone)
        goto bail;

    /*
     * Find the first free catalog entry.  Also returns a pointer to the
     * previous entry.
     */
    dierr = GetFreeCatalogEntry(&catSect, &catEntry, sctBuf, &pPrevEntry);
    if (dierr != kDIErrNone) {
        LOGI("DOS unable to find an empty entry in the catalog");
        goto bail;
    }
    LOGI(" DOS found free catalog entry T=%d S=%d ent=%d prev=0x%08lx",
        catSect.track, catSect.sector, catEntry, (long) pPrevEntry);

    /* create the new dir entry at the specified location */
    CreateDirEntry(sctBuf, catEntry, normalName, &tsSect,
        (uint8_t) fileType, pParms->access);

    /*
     * Flush everything to disk.
     */
    dierr = fpImg->WriteTrackSector(catSect.track, catSect.sector, sctBuf);
    if (dierr != kDIErrNone)
        goto bail;

    dierr = SaveVolBitmap();
    if (dierr != kDIErrNone)
        goto bail;

    /*
     * Create a new entry for our file list.
     */
    pNewFile = new A2FileDOS(this);
    if (pNewFile == NULL) {
        dierr = kDIErrMalloc;
        goto bail;
    }
    pNewFile->fTSListTrack = tsSect.track;
    pNewFile->fTSListSector = tsSect.sector;
    pNewFile->fLengthInSectors = 1;
    pNewFile->fLocked = false;
    strcpy(pNewFile->fFileName, normalName);
    pNewFile->fFileType = fileType;

    pNewFile->fCatTS.track = catSect.track;
    pNewFile->fCatTS.sector = catSect.sector;
    pNewFile->fCatEntryNum = catEntry;

    pNewFile->fAuxType = (uint16_t) pParms->auxType;
    pNewFile->fDataOffset = 0;
    switch (pNewFile->fFileType) {
    case A2FileDOS::kTypeInteger:
        pNewFile->fDataOffset = 2;
        break;
    case A2FileDOS::kTypeApplesoft:
        pNewFile->fDataOffset = 2;
        pNewFile->fAuxType = 0x0801;
        break;
    case A2FileDOS::kTypeBinary:
        pNewFile->fDataOffset = 4;
        break;
    default:
        break;
    }
    pNewFile->fLength = 0;
    pNewFile->fSparseLength = 0;

    /*
     * Insert it in the proper place, so that the order of the files matches
     * the order of entries in the catalog.
     */
    InsertFileInList(pNewFile, pPrevEntry);

    *ppNewFile = pNewFile;
    pNewFile = NULL;

bail:
    delete pNewFile;
    FreeVolBitmap();
    return dierr;
}

/*
 * Make the name pointed to by "fileName" unique.  The name should already
 * be FS-normalized, and be in a buffer that can hold at least kMaxFileName+1
 * bytes.
 *
 * (This is nearly identical to the code in the ProDOS implementation.  I'd
 * like to make it a general DiskFS function, but making the loop condition
 * work requires setting up callbacks, which isn't hard here but is a little
 * annoying in ProDOS because of the subdir buffer.  So it's cut & paste
 * for now.)
 *
 * Returns an error on failure, which should be impossible.
 */
DIError DiskFSDOS33::MakeFileNameUnique(char* fileName)
{
    assert(fileName != NULL);
    assert(strlen(fileName) <= A2FileDOS::kMaxFileName);

    if (GetFileByName(fileName) == NULL)
        return kDIErrNone;

    LOGI(" DOS   found duplicate of '%s', making unique", fileName);

    int nameLen = strlen(fileName);
    int dotOffset=0, dotLen=0;
    char dotBuf[kMaxExtensionLen+1];

    /* ensure the result will be null-terminated */
    memset(fileName + nameLen, 0, (A2FileDOS::kMaxFileName - nameLen) +1);

    /*
     * If this has what looks like a filename extension, grab it.  We want
     * to preserve ".gif", ".c", etc.
     */
    const char* cp = strrchr(fileName, '.');
    if (cp != NULL) {
        int tmpOffset = cp - fileName;
        if (tmpOffset > 0 && nameLen - tmpOffset <= kMaxExtensionLen) {
            LOGI("  DOS   (keeping extension '%s')", cp);
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
        if (nameLen + digitLen > A2FileDOS::kMaxFileName)
            copyOffset = A2FileDOS::kMaxFileName - dotLen - digitLen;
        else
            copyOffset = nameLen - dotLen;
        memcpy(fileName + copyOffset, digitBuf, digitLen);
        if (dotLen != 0)
            memcpy(fileName + copyOffset + digitLen, dotBuf, dotLen);
    } while (GetFileByName(fileName) != NULL);

    LOGI(" DOS  converted to unique name: %s", fileName);

    return kDIErrNone;
}

/*
 * Find the first free entry in the catalog.
 *
 * Also returns an A2File pointer for the previous entry in the catalog.
 *
 * The contents of the catalog sector will be in "sctBuf".
 */
DIError DiskFSDOS33::GetFreeCatalogEntry(TrackSector* pCatSect, int* pCatEntry,
    uint8_t* sctBuf, A2FileDOS** ppPrevEntry)
{
    DIError dierr = kDIErrNone;
    uint8_t* pEntry;
    int sct, ent;
    bool found = false;

    for (sct = 0; sct < kMaxCatalogSectors; sct++) {
        if (fCatalogSectors[sct].track == 0 &&
            fCatalogSectors[sct].sector == 0)
        {
            /* end of list reached */
            LOGI("DOS catalog is full");
            dierr = kDIErrVolumeDirFull;
            goto bail;
        }
        dierr = fpImg->ReadTrackSector(fCatalogSectors[sct].track,
                    fCatalogSectors[sct].sector, sctBuf);
        if (dierr != kDIErrNone)
            goto bail;

        pEntry = &sctBuf[kCatalogEntryOffset];
        for (ent = 0; ent < kCatalogEntriesPerSect; ent++) {
            if (pEntry[0x00] == 0x00 || pEntry[0x00] == kEntryDeleted) {
                /* winner! */
                *pCatSect = fCatalogSectors[sct];
                *pCatEntry = ent;
                found = true;
                break;
            }

            pEntry += kCatalogEntrySize;
        }

        if (found)
            break;
    }

    if (sct == kMaxCatalogSectors) {
        /* didn't find anything, assume the disk is full */
        dierr = kDIErrVolumeDirFull;
        // fall through to "bail"
    } else {
        /* figure out what the previous entry is */
        TrackSector prevTS;
        int prevEntry;

        if (*pCatEntry != 0) {
            prevTS = *pCatSect;
            prevEntry = *pCatEntry -1;
        } else if (sct != 0) {
            prevTS = fCatalogSectors[sct-1];
            prevEntry = kCatalogEntriesPerSect-1;
        } else {
            /* disk was empty; there's no previous entry */
            prevTS.track = 0;
            prevTS.sector = 0;
            prevEntry = -1;
        }

        /* now find it in the linear file list */
        *ppPrevEntry = NULL;
        if (prevEntry >= 0) {
            A2FileDOS* pFile = (A2FileDOS*) GetNextFile(NULL);
            while (pFile != NULL) {
                if (pFile->fCatTS.track == prevTS.track &&
                    pFile->fCatTS.sector == prevTS.sector &&
                    pFile->fCatEntryNum == prevEntry)
                {
                    *ppPrevEntry = pFile;
                    break;
                }
                pFile = (A2FileDOS*) GetNextFile(pFile);
            }
            assert(*ppPrevEntry != NULL);
        }
    }

bail:
    return dierr;
}

/*
 * Fill out the catalog entry in the location specified.
 */
void DiskFSDOS33::CreateDirEntry(uint8_t* sctBuf, int catEntry,
    const char* fileName, TrackSector* pTSSect, uint8_t fileType,
    int access)
{
    char highName[A2FileDOS::kMaxFileName+1];
    uint8_t* pEntry;

    pEntry = GetCatalogEntryPtr(sctBuf, catEntry);
    if (pEntry[0x00] != 0x00 && pEntry[0x00] != kEntryDeleted) {
        /* somebody screwed up */
        assert(false);
        return;
    }

    A2FileDOS::MakeDOSName(highName, fileName);

    pEntry[0x00] = pTSSect->track;
    pEntry[0x01] = pTSSect->sector;
    pEntry[0x02] = fileType;
    if ((access & A2FileProDOS::kAccessWrite) == 0)
        pEntry[0x02] |= (uint8_t) A2FileDOS::kTypeLocked;
    memcpy(&pEntry[0x03], highName, A2FileDOS::kMaxFileName);
    PutShortLE(&pEntry[0x21], 1);       // assume file is 1 sector long
}

/*
 * Delete a file.
 *
 * This entails freeing up the allocated sectors and changing a byte in
 * the directory entry.  We then remove it from the DiskFS file list.
 */
DIError DiskFSDOS33::DeleteFile(A2File* pGenericFile)
{
    DIError dierr = kDIErrNone;
    A2FileDOS* pFile = (A2FileDOS*) pGenericFile;
    TrackSector* tsList = NULL;
    TrackSector* indexList = NULL;
    int tsCount, indexCount;
    uint8_t sctBuf[kSctSize];
    uint8_t* pEntry;

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

    LOGI("    Deleting '%s'", pFile->GetPathName());

    /*
     * Update the block usage map.  Nothing is permanent until we flush
     * the data to disk.
     */
    dierr = LoadVolBitmap();
    if (dierr != kDIErrNone)
        goto bail;

    dierr = pFile->LoadTSList(&tsList, &tsCount, &indexList, &indexCount);
    if (dierr != kDIErrNone) {
        LOGI("Failed loading TS lists while deleting '%s'",
            pFile->GetPathName());
        goto bail;
    }

    FreeTrackSectors(tsList, tsCount);
    FreeTrackSectors(indexList, indexCount);

    /*
     * Mark the entry as deleted.
     */
    dierr = fpImg->ReadTrackSector(pFile->fCatTS.track, pFile->fCatTS.sector,
                sctBuf);
    if (dierr != kDIErrNone)
        goto bail;
    pEntry = GetCatalogEntryPtr(sctBuf, pFile->fCatEntryNum);
    assert(pEntry[0x00] != 0x00 && pEntry[0x00] != kEntryDeleted);
    pEntry[0x00] = kEntryDeleted;
    dierr = fpImg->WriteTrackSector(pFile->fCatTS.track, pFile->fCatTS.sector,
                sctBuf);
    if (dierr != kDIErrNone)
        goto bail;

    /*
     * Save our updated copy of the volume bitmap to disk.
     */
    dierr = SaveVolBitmap();
    if (dierr != kDIErrNone)
        goto bail;

    /*
     * Remove the A2File* from the list.
     */
    DeleteFileFromList(pFile);

bail:
    FreeVolBitmap();
    delete[] tsList;
    delete[] indexList;
    return dierr;
}

/*
 * Mark all of the track/sector entries in "pList" as free.
 */
void DiskFSDOS33::FreeTrackSectors(TrackSector* pList, int count)
{
    VolumeUsage::ChunkState cstate;
    int i;

    cstate.isUsed = false;
    cstate.isMarkedUsed = false;
    cstate.purpose = VolumeUsage::kChunkPurposeUnknown;

    for (i = 0; i < count; i++) {
        if (pList[i].track == 0 && pList[i].sector == 0)
            continue;       // sparse file

        if (!GetSectorUseEntry(pList[i].track, pList[i].sector)) {
            LOGI("WARNING: freeing unallocated sector T=%d S=%d",
                pList[i].track, pList[i].sector);
            assert(false);  // impossible unless disk is "damaged"
        }
        SetSectorUseEntry(pList[i].track, pList[i].sector, false);

        fVolumeUsage.SetChunkState(pList[i].track, pList[i].sector, &cstate);
    }
}

/*
 * Rename a file.
 *
 * "newName" must already be normalized.
 */
DIError DiskFSDOS33::RenameFile(A2File* pGenericFile, const char* newName)
{
    DIError dierr = kDIErrNone;
    A2FileDOS* pFile = (A2FileDOS*) pGenericFile;
    char normalName[A2FileDOS::kMaxFileName+1];
    char dosName[A2FileDOS::kMaxFileName+1];
    uint8_t sctBuf[kSctSize];
    uint8_t* pEntry;

    if (pFile == NULL || newName == NULL)
        return kDIErrInvalidArg;
    if (!IsValidFileName(newName))
        return kDIErrInvalidArg;
    if (fpImg->GetReadOnly())
        return kDIErrAccessDenied;
    if (!fDiskIsGood)
        return kDIErrBadDiskImage;

    LOGI(" DOS renaming '%s' to '%s'", pFile->GetPathName(), newName);

    /*
     * Update the disk catalog entry.
     */
    dierr = fpImg->ReadTrackSector(pFile->fCatTS.track, pFile->fCatTS.sector,
                sctBuf);
    if (dierr != kDIErrNone)
        goto bail;

    pEntry = GetCatalogEntryPtr(sctBuf, pFile->fCatEntryNum);

    DoNormalizePath(newName, '\0', normalName);
    A2FileDOS::MakeDOSName(dosName, normalName);
    memcpy(&pEntry[0x03], dosName, A2FileDOS::kMaxFileName);

    dierr = fpImg->WriteTrackSector(pFile->fCatTS.track, pFile->fCatTS.sector,
                sctBuf);
    if (dierr != kDIErrNone)
        goto bail;

    /*
     * Update our internal copy.
     */
    char storedName[A2FileDOS::kMaxFileName+1];
    strcpy(storedName, dosName);
    LowerASCII((uint8_t*)storedName, A2FileDOS::kMaxFileName);
    A2FileDOS::TrimTrailingSpaces(storedName);

    strcpy(pFile->fFileName, storedName);

bail:
    return dierr;
}

/*
 * Set the file's attributes.
 *
 * We allow the file to be locked or unlocked, and we allow the file type
 * to be changed.  We don't try to rewrite the file if they're changing to or
 * from a format with embedded data (e.g. BAS or BIN); instead, we just
 * change the type letter.  We do need to re-evaluate the end-of-file
 * value afterward.
 *
 * Changing the aux type is only allowed for BIN files.
 */
DIError DiskFSDOS33::SetFileInfo(A2File* pGenericFile, uint32_t fileType,
    uint32_t auxType, uint32_t accessFlags)
{
    DIError dierr = kDIErrNone;
    A2FileDOS* pFile = (A2FileDOS*) pGenericFile;
    TrackSector* tsList = NULL;
    int tsCount;
    bool nowLocked;
    bool typeChanged;

    if (pFile == NULL)
        return kDIErrInvalidArg;
    if (fpImg->GetReadOnly())
        return kDIErrAccessDenied;

    LOGI("DOS SetFileInfo '%s' type=0x%02x aux=0x%04x access=0x%02x",
        pFile->GetPathName(), fileType, auxType, accessFlags);

    /*
     * We can ignore the file/aux type, or we can verify that they're not
     * trying to change it.  The latter is a little more work but makes
     * the API a little more communicative.
     */
    if (!A2FileDOS::IsValidType(fileType)) {
        LOGI("DOS SetFileInfo invalid file type");
        dierr = kDIErrInvalidArg;
        goto bail;
    }
    if (auxType != pFile->GetAuxType() && fileType != 0x06) {
        /* this only makes sense for BIN files */
        LOGI("DOS SetFileInfo aux type mismatch; ignoring");
        //dierr = kDIErrNotSupported;
        //goto bail;
    }

    nowLocked = (accessFlags & A2FileProDOS::kAccessWrite) == 0;
    typeChanged = (fileType != pFile->GetFileType());

    /*
     * Update the file type and locked status, if necessary.
     */
    if (nowLocked != pFile->fLocked || typeChanged) {
        A2FileDOS::FileType newFileType;
        uint8_t sctBuf[kSctSize];
        uint8_t* pEntry;

        LOGI("Updating file '%s'", pFile->GetPathName());

        dierr = fpImg->ReadTrackSector(pFile->fCatTS.track, pFile->fCatTS.sector,
                    sctBuf);
        if (dierr != kDIErrNone)
            goto bail;

        pEntry = GetCatalogEntryPtr(sctBuf, pFile->fCatEntryNum);

        newFileType = A2FileDOS::ConvertFileType(fileType, 0);
        pEntry[0x02] = (uint8_t) newFileType;
        if (nowLocked)
            pEntry[0x02] |= 0x80;

        dierr = fpImg->WriteTrackSector(pFile->fCatTS.track, pFile->fCatTS.sector,
                    sctBuf);
        if (dierr != kDIErrNone)
            goto bail;

        /* update our local copy */
        pFile->fLocked = nowLocked;
    }

    if (!typeChanged && auxType == pFile->GetAuxType()) {
        /* only the locked status has changed; skip the rest */
        goto bail;
    }

    /*
     * If the file has type BIN (either because it was before and we left it
     * alone, or we changed it to BIN), we need to figure out what the aux
     * type should be.  There are two situations:
     *
     * (1) User specified an aux type.  If the aux type passed in doesn't match
     *  what's in the A2FileDOS structure, we assume they meant to change it.
     * (2) User didn't specify an aux type change.  If the file was BIN before,
     *  we don't need to do anything, but if it was just changed to BIN then
     *  we need to extract the aux type from the first sector of the file.
     *
     * There's also a 3rd situation: they changed the aux type for a non-BIN
     * file.  This should have been blocked earlier.
     *
     * On top of all this, if we changed the file type at all then we need to
     * re-scan the file length and "data offset" value.
     */
    uint16_t newAuxType;
    newAuxType = (uint16_t) auxType;

    dierr = pFile->LoadTSList(&tsList, &tsCount);
    if (dierr != kDIErrNone) {
        LOGI(" DOS SFI: unable to load TS list (err=%d)", dierr);
        goto bail;
    }

    if (fileType == 0x06 && tsCount > 0) {
        uint8_t sctBuf[kSctSize];

        dierr = fpImg->ReadTrackSector(tsList[0].track,
                    tsList[0].sector, sctBuf);
        if (dierr != kDIErrNone) {
            LOGI("DOS SFI: unable to get first sector of file");
            goto bail;
        }

        if (auxType == pFile->GetAuxType()) {
            newAuxType = GetShortLE(&sctBuf[0x00]);
            LOGI("  Aux type not changed, extracting from file (0x%04x)",
                newAuxType);
        } else {
            LOGI("  Aux type changed (to 0x%04x), changing file",
                newAuxType);

            PutShortLE(&sctBuf[0x00], newAuxType);
            dierr = fpImg->WriteTrackSector(tsList[0].track,
                        tsList[0].sector, sctBuf);
            if (dierr != kDIErrNone) {
                LOGI("DOS SFI: unable to write first sector of file");
                goto bail;
            }
        }
    } else {
        /* not BIN or file has no sectors */
        if (pFile->fFileType == A2FileDOS::kTypeApplesoft)
            newAuxType = 0x0801;
        else
            newAuxType = 0x0000;
    }

    /* update our local copy */
    pFile->fFileType = A2FileDOS::ConvertFileType(fileType, 0);
    pFile->fAuxType = newAuxType;

    /*
     * Recalculate the file's length and "data offset".  This may also mark
     * the file as "suspicious".  We wouldn't be here if the file was
     * suspicious when we opened the disk image -- the image would have
     * been marked read-only -- so if it's suspicious now, it's probably
     * from a previous file type change attempt in the current session.
     * Clear the flag so it doesn't "stick".
     */
    pFile->ResetQuality();
    (void) ComputeLength(pFile, tsList, tsCount);

bail:
    delete[] tsList;
    return dierr;
}

/*
 * Change the disk volume name (number).
 *
 * We can't change the 2MG header, and we can't change the values embedded
 * in the sector headers, so all we do is change the VTOC entry.
 */
DIError DiskFSDOS33::RenameVolume(const char* newName)
{
    DIError dierr = kDIErrNone;
    uint8_t sctBuf[kSctSize];
    long newNumber;
    char* endp;

    if (!IsValidVolumeName(newName))
        return kDIErrInvalidArg;
    if (fpImg->GetReadOnly())
        return kDIErrAccessDenied;

    // convert the number; we already ascertained that it's valid
    newNumber = strtol(newName, &endp, 10);

    dierr = fpImg->ReadTrackSector(kVTOCTrack, kVTOCSector, sctBuf);
    if (dierr != kDIErrNone)
        goto bail;

    sctBuf[0x06] = (uint8_t) newNumber;

    dierr = fpImg->WriteTrackSector(kVTOCTrack, kVTOCSector, sctBuf);
    if (dierr != kDIErrNone)
        goto bail;

    fVTOCVolumeNumber = newNumber;
    UpdateVolumeNum();

bail:
    return dierr;
}


/*
 * ===========================================================================
 *      A2FileDOS
 * ===========================================================================
 */

/*
 * Constructor.
 */
A2FileDOS::A2FileDOS(DiskFS* pDiskFS) : A2File(pDiskFS)
{
    fTSListTrack = -1;
    fTSListSector = -1;
    fLengthInSectors = 0;
    fLocked = false;
    fFileName[0] = '\0';
    fFileType = kTypeUnknown;

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
A2FileDOS::~A2FileDOS(void)
{
    delete fpOpenFile;
}


/*
 * Convert the filetype enum to a ProDOS type.
 *
 * Remember that the DOS filetype field is actually a bit field, so we need
 * to handle situations where more than one bit is set.
 *
 * Ideally this is a reversible transformation, so files copied to ProDOS
 * volumes can be copied back to DOS with no loss of information.  The reverse
 * is *not* true, because of file type reduction and the potential loss of
 * accurate file length info.
 *
 * I'm not entirely certain about the conversion of 'R' to REL, largely
 * because I can't find any information on the REL format.  However, Copy ][+
 * does convert to REL, and the Binary ][ standard says I should as well.
 */
uint32_t A2FileDOS::GetFileType(void) const
{
    long retval;

    switch (fFileType) {
    case kTypeText:         retval = 0x04;  break;  // TXT
    case kTypeInteger:      retval = 0xfa;  break;  // INT
    case kTypeApplesoft:    retval = 0xfc;  break;  // BAS
    case kTypeBinary:       retval = 0x06;  break;  // BIN
    case kTypeS:            retval = 0xf2;  break;  // $f2
    case kTypeReloc:        retval = 0xfe;  break;  // REL
    case kTypeA:            retval = 0xf3;  break;  // $f3
    case kTypeB:            retval = 0xf4;  break;  // $f4
    case kTypeUnknown:
    default:
        retval = 0x00;      // NON
        break;
    }

    return retval;
}

/*
 * Convert a ProDOS 8 file type to its DOS equivalent.
 *
 * We need to know the file length because files over 64K can't fit into
 * DOS A/I/B files.  Text files can be as long as they want, and the
 * other types don't have a length word defined, so they're fine.
 *
 * We can't just convert them later, because by that point they've already
 * got a 2-byte or 4-byte header reserved.
 *
 * Because we don't generally know the eventual length of the file at
 * the time we're creating it, this doesn't work nearly as well as could
 * be hoped.  We can make life a little less confusing for the caller by
 * using type 'S' for any unknown type.
 */
/*static*/ A2FileDOS::FileType A2FileDOS::ConvertFileType(long prodosType,
    di_off_t fileLen)
{
    const long kMaxBinary = 65535;
    FileType newType;

    switch (prodosType) {
    case 0xb0:      newType = kTypeText;        break;  // SRC
    case 0x04:      newType = kTypeText;        break;  // TXT
    case 0xfa:      newType = kTypeInteger;     break;  // INT
    case 0xfc:      newType = kTypeApplesoft;   break;  // BAS
    case 0x06:      newType = kTypeBinary;      break;  // BIN
    case 0xf2:      newType = kTypeS;           break;  // $f2
    case 0xfe:      newType = kTypeReloc;       break;  // REL
    case 0xf3:      newType = kTypeA;           break;  // $f3
    case 0xf4:      newType = kTypeB;           break;  // $f4
    default:        newType = kTypeS;           break;
    }

    if (fileLen > kMaxBinary &&
        (newType == kTypeInteger || newType == kTypeApplesoft ||
        newType == kTypeBinary))
    {
        LOGI("  DOS setting type for large A/I/B file to S");
        newType = kTypeS;
    }

    return newType;
}

/*
 * Determine whether the specified type has a valid DOS mapping.
 */
/*static*/ bool A2FileDOS::IsValidType(long prodosType)
{
    switch (prodosType) {
    case 0xb0:  // SRC
    case 0x04:  // TXT
    case 0xfa:  // INT
    case 0xfc:  // BAS
    case 0x06:  // BIN
    case 0xf2:  // $f2
    case 0xfe:  // REL
    case 0xf3:  // $f3
    case 0xf4:  // $f4
        return true;
    default:
        return false;
    }
}

/*
 * Match the ProDOS equivalents of "locked" and "unlocked".
 */
uint32_t A2FileDOS::GetAccess(void) const
{
    if (fLocked)
        return DiskFS::kFileAccessLocked;   // 0x01 read
    else
        return DiskFS::kFileAccessUnlocked; // 0xc3 read/write/rename/destroy
}

/*
 * "Fix" a DOS3.3 filename.  Convert DOS-ASCII to normal ASCII, and strip
 * trailing spaces.
 */
void A2FileDOS::FixFilename(void)
{
    DiskFSDOS33::LowerASCII((uint8_t*)fFileName, kMaxFileName);
    TrimTrailingSpaces(fFileName);
}

/*
 * Trim the spaces off the end of a filename.
 *
 * Assumes the filename has already been converted to low ASCII.
 */
/*static*/ void A2FileDOS::TrimTrailingSpaces(char* filename)
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
/*static*/ void A2FileDOS::MakeDOSName(char* buf, const char* name)
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
DIError A2FileDOS::Open(A2FileDescr** ppOpenFile, bool readOnly,
    bool rsrcFork /*=false*/)
{
    DIError dierr = kDIErrNone;
    A2FDDOS* pOpenFile = NULL;

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

    pOpenFile = new A2FDDOS(this);

    dierr = LoadTSList(&pOpenFile->fTSList, &pOpenFile->fTSCount,
                &pOpenFile->fIndexList, &pOpenFile->fIndexCount);
    if (dierr != kDIErrNone) {
        LOGI("DOS33 unable to load TS for '%s' open", GetPathName());
        goto bail;
    }

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
 * Dump the contents of an A2FileDOS.
 */
void A2FileDOS::Dump(void) const
{
    LOGI("A2FileDOS '%s'", fFileName);
    LOGI("  TS T=%-2d S=%-2d", fTSListTrack, fTSListSector);
    LOGI("  Cat T=%-2d S=%-2d", fCatTS.track, fCatTS.sector);
    LOGI("  type=%d lck=%d slen=%d", fFileType, fLocked, fLengthInSectors);
    LOGI("  auxtype=0x%04x length=%ld",
        fAuxType, (long) fLength);
}


/*
 * Load the T/S list for this file.
 *
 * A single T/S sector holds 122 entries, enough to store a 30.5K file.
 * It's very unlikely that a file will need more than two, although it's
 * possible for a random-access text file to have a very large number of
 * entries.
 *
 * If "pIndexList" and "pIndexCount" are non-NULL, the list of index blocks is
 * also loaded.
 *
 * It's entirely possible to get a large T/S list back that is filled
 * entirely with zeroes.  This can happen if we have a large set of T/S
 * index sectors that are all zero.  We have to leave space for them so
 * that the Write function can use the existing allocated index blocks.
 *
 * THOUGHT: we may want to use the file type to tighten this up a bit.
 * For example, we're currently very careful around random-access text
 * files, but if the file doesn't have type 'T' then random access is
 * impossible.  Currently this isn't a problem, but for e.g. T/S lists
 * with garbage at the end would could deal with the problem more generally.
 */
DIError A2FileDOS::LoadTSList(TrackSector** pTSList, int* pTSCount,
    TrackSector** pIndexList, int* pIndexCount)
{
    DIError dierr = kDIErrNone;
    DiskImg* pDiskImg;
    const int kDefaultTSAlloc = 2;
    const int kDefaultIndexAlloc = 8;
    TrackSector* tsList = NULL;
    TrackSector* indexList = NULL;
    int tsCount, tsAlloc;
    int indexCount, indexAlloc;
    uint8_t sctBuf[kSctSize];
    int track, sector, iterations;

    LOGI("--- DOS loading T/S list for '%s'", GetPathName());

    /* over-alloc for small files to reduce reallocs */
    tsAlloc = kMaxTSPairs * kDefaultTSAlloc;
    tsList = new TrackSector[tsAlloc];
    tsCount = 0;

    indexAlloc = kDefaultIndexAlloc;
    indexList = new TrackSector[indexAlloc];
    indexCount = 0;

    if (tsList == NULL || indexList == NULL) {
        dierr = kDIErrMalloc;
        goto bail;
    }

    assert(fpDiskFS != NULL);
    pDiskImg = fpDiskFS->GetDiskImg();
    assert(pDiskImg != NULL);

    /* get the first T/S sector for this file */
    track = fTSListTrack;
    sector = fTSListSector;
    if (track >= pDiskImg->GetNumTracks() ||
        sector >= pDiskImg->GetNumSectPerTrack())
    {
        LOGI(" DOS33 invalid initial T/S %d,%d in '%s'", track, sector,
            fFileName);
        dierr = kDIErrBadFile;
        goto bail;
    }

    /*
     * Run through the set of t/s pairs.
     */
    iterations = 0;
    do {
        uint16_t sectorOffset;
        int lastNonZero;

        /*
         * Add the current T/S sector to the index list.
         */
        if (indexCount == indexAlloc) {
            LOGI("+++ expanding index list");
            TrackSector* newList;
            indexAlloc += kDefaultIndexAlloc;
            newList = new TrackSector[indexAlloc];
            if (newList == NULL) {
                dierr = kDIErrMalloc;
                goto bail;
            }
            memcpy(newList, indexList, indexCount * sizeof(TrackSector));
            delete[] indexList;
            indexList = newList;
        }
        indexList[indexCount].track = track;
        indexList[indexCount].sector = sector;
        indexCount++;


        //LOGI("+++ scanning T/S at T=%d S=%d", track, sector);
        dierr = pDiskImg->ReadTrackSector(track, sector, sctBuf);
        if (dierr != kDIErrNone)
            goto bail;

        /* grab next track/sector */
        track = sctBuf[0x01];
        sector = sctBuf[0x02];
        sectorOffset = GetShortLE(&sctBuf[0x05]);

        /* if T/S link is bogus, whole sector is probably bad */
        if (track >= pDiskImg->GetNumTracks() ||
            sector >= pDiskImg->GetNumSectPerTrack())
        {
            // bogus T/S, mark file as damaged and stop
            LOGI(" DOS33 invalid T/S link %d,%d in '%s'", track, sector,
                GetPathName());
            dierr = kDIErrBadFile;
            goto bail;
        }
        if ((sectorOffset % kMaxTSPairs) != 0) {
            LOGI(" DOS33 invalid T/S header sector offset %u in '%s'",
                sectorOffset, GetPathName());
            // not fatal, just weird
        }

        /*
         * Make sure we have enough room to hold an entire sector full of
         * T/S pairs in the list.
         */
        if (tsCount + kMaxTSPairs > tsAlloc) {
            LOGI("+++ expanding ts list");
            TrackSector* newList;
            tsAlloc += kMaxTSPairs * kDefaultTSAlloc;
            newList = new TrackSector[tsAlloc];
            if (newList == NULL) {
                dierr = kDIErrMalloc;
                goto bail;
            }
            memcpy(newList, tsList, tsCount * sizeof(TrackSector));
            delete[] tsList;
            tsList = newList;
        }

        /*
         * Add the entries.  If there's another T/S list linked, we just
         * grab the entire sector.  If not, we grab every entry until the
         * last 0,0.  (Can't stop at the first (0,0), or we'll drop a
         * piece of a random access text file.)
         */
        dierr = ExtractTSPairs(sctBuf, &tsList[tsCount], &lastNonZero);
        if (dierr != kDIErrNone)
            goto bail;

        if (track != 0 && sector != 0) {
            /* more T/S lists to come, so we keep all entries */
            tsCount += kMaxTSPairs;
        } else {
            /* this was the last one */
            if (lastNonZero == -1) {
                /* this is ALWAYS the case for a newly-created file */
                //LOGI(" DOS33 odd -- last T/S sector of '%s' was empty",
                //  GetPathName());
            }
            tsCount += lastNonZero +1;
        }

        iterations++;       // watch for infinite loops
    } while (!(track == 0 && sector == 0) && iterations < kMaxTSIterations);

    if (iterations == kMaxTSIterations) {
        dierr = kDIErrFileLoop;
        goto bail;
    }

    *pTSList = tsList;
    *pTSCount = tsCount;
    tsList = NULL;

    if (pIndexList != NULL) {
        *pIndexList = indexList;
        *pIndexCount = indexCount;
        indexList = NULL;
    }

bail:
    delete[] tsList;
    delete[] indexList;
    return dierr;
}

/*
 * Extract the track/sector pairs from the TS list in "sctBuf".  The entries
 * are copied to "tsList", which is assumed to have enough space to hold
 * at least kMaxTSPairs entries.
 *
 * The last non-zero entry will be identified and stored in "*pLastNonZero".
 * If all entries are zero, it will be set to -1.
 *
 * Sometimes files will have junk at the tail end of an otherwise valid
 * T/S list.  We can't just stop when we hit the first (0,0) entry because
 * that'll screw up random-access text file handling.  What we can do is
 * try to detect the situation, and mark the file as "suspicious" without
 * returning an error if we see it.
 *
 * If a TS entry appears to be invalid, this returns an error after all
 * entries have been copied.  If it looks to be partially valid, only the
 * valid parts are copied out, with the rest zeroed.
 */
DIError A2FileDOS::ExtractTSPairs(const uint8_t* sctBuf, TrackSector* tsList,
    int* pLastNonZero)
{
    DIError dierr = kDIErrNone;
    const DiskImg* pDiskImg = fpDiskFS->GetDiskImg();
    const uint8_t* ptr;
    int i, track, sector;

    *pLastNonZero = -1;
    memset(tsList, 0, sizeof(TrackSector) * kMaxTSPairs);

    ptr = &sctBuf[kTSOffset];       // offset of first T/S entry (0x0c)

    for (i = 0; i < kMaxTSPairs; i++) {
        track = *ptr++;
        sector = *ptr++;

        if (dierr == kDIErrNone &&
            (track >= pDiskImg->GetNumTracks() ||
             sector >= pDiskImg->GetNumSectPerTrack() ||
             (track == 0 && sector != 0)))
        {
            LOGI(" DOS33 invalid T/S %d,%d in '%s'", track, sector,
                fFileName);

            if (i > 0 && tsList[i-1].track == 0 && tsList[i-1].sector == 0) {
                LOGI("  T/S list looks partially valid");
                SetQuality(kQualitySuspicious);
                goto bail;  // quit immediately
            } else {
                dierr = kDIErrBadFile;
                // keep going, just so caller has the full set to stare at
            }
        }

        if (track != 0 || sector != 0)
            *pLastNonZero = i;

        tsList[i].track = track;
        tsList[i].sector = sector;
    }

bail:
    return dierr;
}


/*
 * ===========================================================================
 *      A2FDDOS
 * ===========================================================================
 */

/*
 * Read data from the current offset.
 *
 * Files read back as they would from ProDOS, i.e. if you read a binary
 * file you won't see the 4 bytes of length and address.
 */
DIError A2FDDOS::Read(void* buf, size_t len, size_t* pActual)
{
    LOGD(" DOS reading %lu bytes from '%s' (offset=%ld)",
        (unsigned long) len, fpFile->GetPathName(), (long) fOffset);

    A2FileDOS* pFile = (A2FileDOS*) fpFile;

    /*
     * Don't allow them to read past the end of the file.  The length value
     * stored in pFile->fLength already has pFile->fDataOffset subtracted
     * from the actual data length, so don't factor it in again.
     */
    if (fOffset + (long)len > fOpenEOF) {
        if (pActual == NULL)
            return kDIErrDataUnderrun;
        len = (size_t) (fOpenEOF - fOffset);
    }
    if (pActual != NULL)
        *pActual = len;
    long incrLen = len;

    DIError dierr = kDIErrNone;
    uint8_t sctBuf[kSctSize];
    di_off_t actualOffset = fOffset + pFile->fDataOffset;   // adjust for embedded len
    int tsIndex = (int) (actualOffset / kSctSize);
    int bufOffset = (int) (actualOffset % kSctSize);        // (& 0xff)
    size_t thisCount;

    if (len == 0)
        return kDIErrNone;
    assert(fOpenEOF != 0);

    assert(tsIndex >= 0 && tsIndex < fTSCount);

    /* could be more clever in here and avoid double-buffering */
    while (len) {
        if (tsIndex >= fTSCount) {
            /* should've caught this earlier */
            assert(false);
            LOGI(" DOS ran off the end (fTSCount=%d)", fTSCount);
            return kDIErrDataUnderrun;
        }

        if (fTSList[tsIndex].track == 0 && fTSList[tsIndex].sector == 0) {
            //LOGI(" DOS sparse sector T=%d S=%d",
            //  TSTrack(fTSList[tsIndex]), TSSector(fTSList[tsIndex]));
            memset(sctBuf, 0, sizeof(sctBuf));
        } else {
            dierr = pFile->GetDiskFS()->GetDiskImg()->ReadTrackSector(
                        fTSList[tsIndex].track,
                        fTSList[tsIndex].sector,
                        sctBuf);
            if (dierr != kDIErrNone) {
                LOGI(" DOS error reading file '%s'", pFile->GetPathName());
                return dierr;
            }
        }
        thisCount = kSctSize - bufOffset;
        if (thisCount > len)
            thisCount = len;
        memcpy(buf, sctBuf + bufOffset, thisCount);
        len -= thisCount;
        buf = (char*)buf + thisCount;

        bufOffset = 0;
        tsIndex++;
    }

    fOffset += incrLen;

    return dierr;
}

/*
 * Write data at the current offset.
 *
 * For simplicity, we assume that we're writing a brand-new file in one
 * shot.  As it happens, that's all we're currently required to do, so even
 * if we wrote a more sophisticated function it wouldn't get exercised.
 * Because of the way we write, there's no way to mimic the behavior of
 * random-access text file allocation, so that isn't supported.
 *
 * The data in "buf" should *not* include the 2-4 bytes of header present
 * on A/I/B files.  That's already factored in.
 *
 * Modifies fOpenEOF, fOpenSectorsUsed, and sets fModified.
 */
DIError A2FDDOS::Write(const void* buf, size_t len, size_t* pActual)
{
    DIError dierr = kDIErrNone;
    A2FileDOS* pFile = (A2FileDOS*) fpFile;
    DiskFSDOS33* pDiskFS = (DiskFSDOS33*) fpFile->GetDiskFS();
    uint8_t sctBuf[kSctSize];

    LOGD("   DOS Write len=%lu %s", (unsigned long) len, pFile->GetPathName());

    if (len >= 0x01000000) {    // 16MB
        assert(false);
        return kDIErrInvalidArg;
    }
    assert(fOffset == 0);       // big simplifying assumption
    assert(fOpenEOF == 0);      // another one
    assert(fTSCount == 0);      // must hold for our newly-created files
    assert(fIndexCount == 1);   // must hold for our newly-created files
    assert(fOpenSectorsUsed == fTSCount + fIndexCount);
    assert(buf != NULL);

    long actualLen = (long) len + pFile->fDataOffset;
    long numSectors = (actualLen + kSctSize -1) / kSctSize;
    TrackSector firstIndex;
    int i;

    /*
     * Nothing to do for zero-length write; don't even set fModified.  Note,
     * however, that a zero-length 'B' file is actually 4 bytes long, and
     * must have a data block allocated.
     */
    if (actualLen == 0)
        goto bail;
    assert(numSectors > 0);

    dierr = pDiskFS->LoadVolBitmap();
    if (dierr != kDIErrNone)
        goto bail;

    /*
     * Start by allocating a full T/S list.  The existing T/S list is
     * empty, but we do have one T/S index sector to fill before we
     * allocate any others.
     *
     * Since we determined above that there was nothing interesting in
     * our T/S list, we just grab the one allocated block, throw out
     * the lists, and reallocate them.
     */
    firstIndex = fIndexList[0];
    delete[] fTSList;
    delete[] fIndexList;
    fTSList = fIndexList = NULL;

    fTSCount = numSectors;
    fTSList = new TrackSector[fTSCount];
    fIndexCount = (numSectors + kMaxTSPairs -1) / kMaxTSPairs;
    assert(fIndexCount > 0);
    fIndexList = new TrackSector[fIndexCount];
    if (fTSList == NULL || fIndexList == NULL) {
        dierr = kDIErrMalloc;
        goto bail;
    }

    /*
     * Allocate all of the index sectors.  In theory we should to this along
     * with the file sectors, so that the index and file sectors are
     * interspersed with the data, but in practice 99% of the file have
     * only one or two index blocks.  By grouping them together we improve
     * the performance for emulators and CiderPress.
     */
    fIndexList[0] = firstIndex;
    for (i = 1; i < fIndexCount; i++) {
        TrackSector allocTS;

        dierr = pDiskFS->AllocSector(&allocTS);
        if (dierr != kDIErrNone)
            goto bail;
        fIndexList[i] = allocTS;
    }
    /*
     * Allocate the data sectors.
     */
    for (i = 0; i < fTSCount; i++) {
        TrackSector allocTS;

        dierr = pDiskFS->AllocSector(&allocTS);
        if (dierr != kDIErrNone)
            goto bail;
        fTSList[i] = allocTS;
    }

    /*
     * Write the sectors into the T/S list.
     */
    const uint8_t* curPtr;
    int sectorIdx;

    curPtr = (const uint8_t*) buf;
    sectorIdx = 0;

    if (pFile->fDataOffset > 0) {
        /* handle first sector specially */
        assert(pFile->fDataOffset < kSctSize);
        int dataInFirstSct = kSctSize - pFile->fDataOffset;
        if (dataInFirstSct > actualLen - pFile->fDataOffset)
            dataInFirstSct = actualLen - pFile->fDataOffset;

        // dataInFirstSct could be zero (== len)
        memset(sctBuf, 0, sizeof(sctBuf));
        memcpy(sctBuf + pFile->fDataOffset, curPtr,
            dataInFirstSct);
        dierr = pDiskFS->GetDiskImg()->WriteTrackSector(fTSList[sectorIdx].track,
                    fTSList[sectorIdx].sector, sctBuf);
        if (dierr != kDIErrNone)
            goto bail;

        sectorIdx++;
        actualLen -= dataInFirstSct + pFile->fDataOffset;
        curPtr += dataInFirstSct;
    }
    while (actualLen > 0) {
        if (actualLen >= kSctSize) {
            /* write directly from input */
            dierr = pDiskFS->GetDiskImg()->WriteTrackSector(fTSList[sectorIdx].track,
                fTSList[sectorIdx].sector, curPtr);
            if (dierr != kDIErrNone)
                goto bail;
        } else {
            /* make a copy of the partial buffer */
            memset(sctBuf, 0, sizeof(sctBuf));
            memcpy(sctBuf, curPtr, actualLen);
            dierr = pDiskFS->GetDiskImg()->WriteTrackSector(fTSList[sectorIdx].track,
                fTSList[sectorIdx].sector, sctBuf);
            if (dierr != kDIErrNone)
                goto bail;
        }

        sectorIdx++;
        actualLen -= kSctSize;  // goes negative; that's fine
        curPtr += kSctSize;
    }
    assert(sectorIdx == fTSCount);

    /*
     * Fill out the T/S list sectors.  Failure here presents a potential
     * problem because, once we've written the first T/S entry, the file
     * appears to have storage that it actually doesn't.  The easiest way
     * to handle this safely is to start by writing the last index block
     * first.
     */
    for (i = fIndexCount-1; i >= 0; i--) {
        int tsOffset = i * kMaxTSPairs;
        assert(tsOffset < fTSCount);

        memset(sctBuf, 0, kSctSize);
        if (i != fIndexCount-1) {
            sctBuf[0x01] = fIndexList[i+1].track;
            sctBuf[0x02] = fIndexList[i+1].sector;
        }
        PutShortLE(&sctBuf[0x05], kMaxTSPairs * i);

        int ent = i * kMaxTSPairs;      // start here
        for (int j = 0; j < kMaxTSPairs; j++) {
            if (ent == fTSCount)
                break;
            sctBuf[kTSOffset + j*2] = fTSList[ent].track;
            sctBuf[kTSOffset + j*2 +1] = fTSList[ent].sector;
            ent++;
        }

        dierr = pDiskFS->GetDiskImg()->WriteTrackSector(fIndexList[i].track,
                    fIndexList[i].sector, sctBuf);
        if (dierr != kDIErrNone)
            goto bail;
    }

    dierr = pDiskFS->SaveVolBitmap();
    if (dierr != kDIErrNone) {
        /*
         * This is awkward -- we wrote the first T/S list, so the file
         * now appears to have content, but the blocks aren't marked used.
         * We read the VTOC successfully though, so it's VERY unlikely
         * that this will fail.  If it does, it's likely that any attempt
         * to mitigate the problem will also fail.  (Maybe we could force
         * the object into read-only mode?)
         */
        goto bail;
    }

    /* finish up */
    fOpenSectorsUsed = fIndexCount + fTSCount;
    fOpenEOF = len;
    fOffset += len;
    fModified = true;

    if (!UpdateProgress(fOffset))
        dierr = kDIErrCancelled;

bail:
    pDiskFS->FreeVolBitmap();
    return dierr;
}

/*
 * Seek to the specified offset.
 */
DIError A2FDDOS::Seek(di_off_t offset, DIWhence whence)
{
    //di_off_t fileLength = fpFile->GetDataLength();

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
    return kDIErrNone;
}

/*
 * Return current offset.
 */
di_off_t A2FDDOS::Tell(void)
{
    return fOffset;
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
DIError A2FDDOS::Close(void)
{
    DIError dierr = kDIErrNone;

    if (fModified) {
        DiskFSDOS33* pDiskFS = (DiskFSDOS33*) fpFile->GetDiskFS();
        A2FileDOS* pFile = (A2FileDOS*) fpFile;
        uint8_t sctBuf[kSctSize];
        uint8_t* pEntry;

        /*
         * Fill in the length and address, if needed for this type of file.
         */
        if (pFile->fFileType == A2FileDOS::kTypeInteger ||
            pFile->fFileType == A2FileDOS::kTypeApplesoft ||
            pFile->fFileType == A2FileDOS::kTypeBinary)
        {
            assert(fTSCount > 0);
            assert(pFile->fDataOffset > 0);
            //assert(fOpenEOF < 65536);
            if (fOpenEOF > 65535) {
                LOGW("WARNING: DOS Close trimming A/I/B file from %ld to 65535",
                    (long) fOpenEOF);
                fOpenEOF = 65535;
            }
            dierr = pDiskFS->GetDiskImg()->ReadTrackSector(fTSList[0].track,
                        fTSList[0].sector, sctBuf);
            if (dierr != kDIErrNone) {
                LOGW("DOS Close: unable to get first sector of file");
                goto bail;
            }

            if (pFile->fFileType == A2FileDOS::kTypeInteger ||
                pFile->fFileType == A2FileDOS::kTypeApplesoft)
            {
                PutShortLE(&sctBuf[0x00], (uint16_t) fOpenEOF);
            } else {
                PutShortLE(&sctBuf[0x00], pFile->fAuxType);
                PutShortLE(&sctBuf[0x02], (uint16_t) fOpenEOF);
            }

            dierr = pDiskFS->GetDiskImg()->WriteTrackSector(fTSList[0].track,
                        fTSList[0].sector, sctBuf);
            if (dierr != kDIErrNone) {
                LOGW("DOS Close: unable to write first sector of file");
                goto bail;
            }
        } else if (pFile->fFileType == A2FileDOS::kTypeText) {
            /*
             * The length of text files can be determined by looking for the
             * first $00.  A file of exactly 256 bytes occupies only one
             * sector though, so running out of sectors also works -- the
             * last $00 is not mandatory.
             *
             * Bottom line is that the value we just wrote for fOpenEOF is
             * *probably* recoverable, so we can stuff it into "fLength"
             * with some assurance that it will be there when we reopen the
             * file.
             */
        } else {
            /*
             * The remaining file types have a length based solely on
             * sector count.  We need to round off our length value.
             */
            fOpenEOF = ((fOpenEOF + kSctSize-1) / kSctSize) * kSctSize;
        }

        /*
         * Update our internal copies of stuff.
         */
        pFile->fLength = fOpenEOF;
        pFile->fSparseLength = pFile->fLength;
        pFile->fLengthInSectors = (uint16_t) fOpenSectorsUsed;

        /*
         * Update the sector count in the directory entry.
         */
        dierr = pDiskFS->GetDiskImg()->ReadTrackSector(pFile->fCatTS.track,
                    pFile->fCatTS.sector, sctBuf);
        if (dierr != kDIErrNone)
            goto bail;

        pEntry = GetCatalogEntryPtr(sctBuf, pFile->fCatEntryNum);
        assert(GetShortLE(&pEntry[0x21]) == 1);     // holds for new file
        PutShortLE(&pEntry[0x21], pFile->fLengthInSectors);
        dierr = pDiskFS->GetDiskImg()->WriteTrackSector(pFile->fCatTS.track,
                    pFile->fCatTS.sector, sctBuf);
    }

bail:
    fpFile->CloseDescr(this);
    return dierr;
}


/*
 * Return the #of sectors/blocks in the file.
 */
long A2FDDOS::GetSectorCount(void) const
{
    return fTSCount;
}

long A2FDDOS::GetBlockCount(void) const
{
    return (fTSCount+1)/2;
}

/*
 * Return the Nth track/sector in this file.
 *
 * Returns (0,0) for a sparse sector.
 */
DIError A2FDDOS::GetStorage(long sectorIdx, long* pTrack, long* pSector) const
{
    if (sectorIdx < 0 || sectorIdx >= fTSCount)
        return kDIErrInvalidIndex;

    *pTrack = fTSList[sectorIdx].track;
    *pSector = fTSList[sectorIdx].sector;
    return kDIErrNone;
}
/*
 * Return the Nth 512-byte block in this file.  Since things aren't stored
 * in 512-byte blocks, we're reduced to finding storage at (tsIndex*2) and
 * converting it to a block number.
 */
DIError A2FDDOS::GetStorage(long blockIdx, long* pBlock) const
{
    long sectorIdx = blockIdx * 2;
    if (sectorIdx < 0 || sectorIdx >= fTSCount)
        return kDIErrInvalidIndex;

    bool dummy;
    TrackSectorToBlock(fTSList[sectorIdx].track,
        fTSList[sectorIdx].sector, pBlock, &dummy);
    assert(*pBlock < fpFile->GetDiskFS()->GetDiskImg()->GetNumBlocks());
    return kDIErrNone;
}


/*
 * Dump the T/S list for an open file.
 */
void A2FDDOS::DumpTSList(void) const
{
    //A2FileDOS* pFile = (A2FileDOS*) fpFile;
    LOGI(" DOS T/S list for '%s' (count=%d)",
        ((A2FileDOS*)fpFile)->fFileName, fTSCount);

    int i;
    for (i = 0; i <= fTSCount; i++) {
        LOGI(" %3d: T=%-2d S=%d", i, fTSList[i].track, fTSList[i].sector);
    }
}
