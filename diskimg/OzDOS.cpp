/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Implementation of DiskFSOzDOS class.
 *
 * It would make life MUCH EASIER to have the DiskImg recognize this as
 * a file format and just rearrange the blocks into linear order for us,
 * but unfortunately that's not going to happen.
 */
#include "StdAfx.h"
#include "DiskImgPriv.h"


/*
 * ===========================================================================
 *      DiskFSOzDOS
 * ===========================================================================
 */

const int kExpectedNumBlocks = 1600;
const int kExpectedTracks = 50;         // 50 tracks of 32 sectors == 400K
const int kExpectedSectors = 32;
const int kVTOCTrack = 17;
const int kVTOCSector = 0;
const int kSctSize = 256;

const int kCatalogEntrySize = 0x23;     // length in bytes of catalog entries
const int kCatalogEntriesPerSect = 7;   // #of entries per catalog sector
const int kMaxTSPairs = 0x7a;           // 122 entries for 256-byte sectors
const int kTSOffset = 0x0c;             // first T/S entry in a T/S list

const int kMaxTSIterations = 32;
const int kMaxCatalogIterations = 64;


/*
 * Read a track/sector, adjusting for 32-sector disks being treated as
 * if they were 16-sector.
 */
static DIError
ReadTrackSectorAdjusted(DiskImg* pImg, int track, int sector,
    int sectorOffset, unsigned char* buf, DiskImg::SectorOrder imageOrder)
{
    track *= 4;
    sector = sector * 2 + sectorOffset;
    while (sector >= 16) {
        track++;
        sector -= 16;
    }
    return pImg->ReadTrackSectorSwapped(track, sector, buf, imageOrder,
            DiskImg::kSectorOrderDOS);
}

/*
 * Test for presence of 400K OzDOS 3.3 volumes.
 */
static DIError
TestImageHalf(DiskImg* pImg, int sectorOffset, DiskImg::SectorOrder imageOrder)
{
    DIError dierr = kDIErrNone;
    unsigned char sctBuf[kSctSize];
    int numTracks, numSectors;
    int catTrack, catSect;
    int foundGood = 0;
    int iterations = 0;

    assert(sectorOffset == 0 || sectorOffset == 1);

    dierr = ReadTrackSectorAdjusted(pImg, kVTOCTrack, kVTOCSector,
                sectorOffset, sctBuf, imageOrder);
    if (dierr != kDIErrNone)
        goto bail;

    catTrack = sctBuf[0x01];
    catSect = sctBuf[0x02];
    numTracks = sctBuf[0x34];
    numSectors = sctBuf[0x35];

    if (!(sctBuf[0x27] == kMaxTSPairs) ||
        /*!(sctBuf[0x36] == 0 && sctBuf[0x37] == 1) ||*/    // bytes per sect
        !(numTracks == kExpectedTracks) ||
        !(numSectors == 32) ||
        !(catTrack < numTracks && catSect < numSectors) ||
        0)
    {
        WMSG1("  OzDOS header test %d failed\n", sectorOffset);
        dierr = kDIErrFilesystemNotFound;
        goto bail;
    }

    /*
     * Walk through the catalog track to try to figure out ordering.
     */
    while (catTrack != 0 && catSect != 0 && iterations < kMaxCatalogIterations)
    {
        dierr = ReadTrackSectorAdjusted(pImg, catTrack, catSect,
                    sectorOffset, sctBuf, imageOrder);
        if (dierr != kDIErrNone)
            goto bail_ok;       /* allow it if not fully broken */

        if (sctBuf[1] == catTrack && sctBuf[2] == catSect-1)
            foundGood++;

        catTrack = sctBuf[1];
        catSect = sctBuf[2];
        iterations++;       // watch for infinite loops
    }
    if (iterations >= kMaxCatalogIterations) {
        dierr = kDIErrDirectoryLoop;
        goto bail;
    }

bail_ok:
    WMSG3(" OzDOS foundGood=%d off=%d swap=%d\n", foundGood, sectorOffset,
        imageOrder);
    /* foundGood hits 3 even when swap is wrong */
    if (foundGood > 4)
        dierr = kDIErrNone;
    else
        dierr = kDIErrFilesystemNotFound;

bail:
    return dierr;
}

/*
 * Test both of the DOS partitions.
 */
static DIError
TestImage(DiskImg* pImg, DiskImg::SectorOrder imageOrder)
{
    DIError dierr;

    WMSG1(" OzDOS checking first half (swap=%d)\n", imageOrder);
    dierr = TestImageHalf(pImg, 0, imageOrder);
    if (dierr != kDIErrNone)
        return dierr;

    WMSG1(" OzDOS checking second half (swap=%d)\n", imageOrder);
    dierr = TestImageHalf(pImg, 1, imageOrder);
    if (dierr != kDIErrNone)
        return dierr;

    return kDIErrNone;
}

/*
 * Test to see if the image is a OzDOS volume.
 */
/*static*/ DIError
DiskFSOzDOS::TestFS(DiskImg* pImg, DiskImg::SectorOrder* pOrder,
    DiskImg::FSFormat* pFormat, FSLeniency leniency)
{
    /* only on 800K disks (at the least, insist on numTracks being even) */
    if (pImg->GetNumBlocks() != kExpectedNumBlocks)
        return kDIErrFilesystemNotFound;

    /* if a value is specified, try that first -- useful for OverrideFormat */
    if (*pOrder != DiskImg::kSectorOrderUnknown) {
        if (TestImage(pImg, *pOrder) == kDIErrNone) {
            WMSG0(" OzDOS accepted FirstTry value\n");
            return kDIErrNone;
        }
    }

    DiskImg::SectorOrder ordering[DiskImg::kSectorOrderMax];
    
    DiskImg::GetSectorOrderArray(ordering, *pOrder);

    for (int i = 0; i < DiskImg::kSectorOrderMax; i++) {
        if (ordering[i] == DiskImg::kSectorOrderUnknown)
            continue;
        if (TestImage(pImg, ordering[i]) == kDIErrNone) {
            *pOrder = ordering[i];
            *pFormat = DiskImg::kFormatOzDOS;
            return kDIErrNone;
        }
    }

    WMSG0(" OzDOS didn't find valid FS\n");
    return kDIErrFilesystemNotFound;
}

#if 0
/*
 * Test to see if the image is a 'wide' (32-sector) DOS3.3 volume, i.e.
 * half of a OzDOS volume.
 */
/*static*/ DIError
DiskFS::TestOzWideDOS33(const DiskImg* pImg, DiskImg::SectorOrder* pOrder)
{
    DIError dierr = kDIErrNone;

    /* only on 400K disks (at the least, insist on numTracks being even) */
    if (pImg->GetNumBlocks() != kExpectedNumBlocks/2)
        return kDIErrFilesystemNotFound;

    /* if a value is specified, try that first -- useful for OverrideFormat */
    if (*pOrder != DiskImg::kSectorOrderUnknown) {
        if (TestImageHalf(pImg, 0, *pOrder) == kDIErrNone) {
            WMSG0(" WideDOS accepted FirstTry value\n");
            return kDIErrNone;
        }
    }

    if (TestImageHalf(pImg, 0, DiskImg::kSectorOrderDOS) == kDIErrNone) {
        *pOrder = DiskImg::kSectorOrderDOS;
    } else if (TestImageHalf(pImg, 0, DiskImg::kSectorOrderProDOS) == kDIErrNone) {
        *pOrder = DiskImg::kSectorOrderProDOS;
    } else if (TestImageHalf(pImg, 0, DiskImg::kSectorOrderPhysical) == kDIErrNone) {
        *pOrder = DiskImg::kSectorOrderPhysical;
    } else {
        WMSG0("  FS didn't find valid 'wide' DOS3.3\n");
        return kDIErrFilesystemNotFound;
    }

    return kDIErrNone;
}
#endif

/*
 * Set up our sub-volumes.
 */
DIError
DiskFSOzDOS::Initialize(void)
{
    DIError dierr = kDIErrNone;

    if (fScanForSubVolumes != kScanSubDisabled) {
        dierr = OpenSubVolume(0);
        if (dierr != kDIErrNone)
            return dierr;

        dierr = OpenSubVolume(1);
        if (dierr != kDIErrNone)
            return dierr;
    } else {
        WMSG0(" OzDOS not scanning for sub-volumes\n");
    }

    SetVolumeUsageMap();

    return kDIErrNone;
}

/*
 * Open up one of the DOS 3.3 sub-volumes.
 */
DIError
DiskFSOzDOS::OpenSubVolume(int idx)
{
    DIError dierr = kDIErrNone;
    DiskFS* pNewFS = NULL;
    DiskImg* pNewImg = NULL;
    
    pNewImg = new DiskImg;
    if (pNewImg == NULL) {
        dierr = kDIErrMalloc;
        goto bail;
    }

    // open the full 800K; SetPairedSectors cuts it in half
    dierr = pNewImg->OpenImage(fpImg, 0, 0,
                2 * kExpectedTracks * kExpectedSectors);
    if (dierr != kDIErrNone) {
        WMSG3(" OzSub: OpenImage(%d,0,%d) failed (err=%d)\n",
            0, 2 * kExpectedTracks * kExpectedSectors, dierr);
        goto bail;
    }

    assert(idx == 0 || idx == 1);
    pNewImg->SetPairedSectors(true, 1-idx);

    WMSG1(" OzSub: testing for recognizable volume in idx=%d\n", idx);
    dierr = pNewImg->AnalyzeImage();
    if (dierr != kDIErrNone) {
        WMSG1(" OzSub: analysis failed (err=%d)\n", dierr);
        goto bail;
    }

    if (pNewImg->GetFSFormat() == DiskImg::kFormatUnknown ||
        pNewImg->GetSectorOrder() == DiskImg::kSectorOrderUnknown)
    {
        WMSG0(" OzSub: unable to identify filesystem\n");
        dierr = kDIErrUnsupportedFSFmt;
        goto bail;
    }

    /* open a DiskFS for the sub-image */
    WMSG1(" UNISub %d succeeded!\n", idx);
    pNewFS = pNewImg->OpenAppropriateDiskFS();
    if (pNewFS == NULL) {
        WMSG0(" OzSub: OpenAppropriateDiskFS failed\n");
        dierr = kDIErrUnsupportedFSFmt;
        goto bail;
    }

    /* load the files from the sub-image */
    dierr = pNewFS->Initialize(pNewImg, kInitFull);
    if (dierr != kDIErrNone) {
        WMSG1(" OzSub: error %d reading list of files from disk", dierr);
        goto bail;
    }

    /* if this really is DOS 3.3, override the "volume name" */
    if (pNewImg->GetFSFormat() == DiskImg::kFormatDOS33) {
        DiskFSDOS33* pDOS = (DiskFSDOS33*) pNewFS;  /* eek, a downcast */
        pDOS->SetDiskVolumeNum(idx+1);
    }

    /*
     * Success, add it to the sub-volume list.
     */
    AddSubVolumeToList(pNewImg, pNewFS);

bail:
    if (dierr != kDIErrNone) {
        delete pNewFS;
        delete pNewImg;
    }
    return dierr;
}
