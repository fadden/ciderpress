/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Implementation of DiskFSUNIDOS class.
 *
 * The "UNIDOS" filesystem doesn't actually hold files.  Instead, it holds
 * two 400K DOS 3.3 volumes on an 800K disk.
 *
 * We do have a test here for "wide" DOS 3.3, which is largely a clone of
 * the standard DOS 3.3 test.  The trick is that we have to adjust our
 * detection to account for 32-sector tracks, and do so while the object
 * is still in a state where it believes it has 16 sectors per track.
 */
#include "StdAfx.h"
#include "DiskImgPriv.h"


/*
 * ===========================================================================
 *      DiskFSUNIDOS
 * ===========================================================================
 */

const int kExpectedNumBlocks = 1600;
const int kExpectedTracks = 50;
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
static DIError ReadTrackSectorAdjusted(DiskImg* pImg, int track, int sector,
    int trackOffset, uint8_t* buf, DiskImg::SectorOrder imageOrder)
{
    track += trackOffset;
    track *= 2;
    if (sector >= 16) {
        track++;
        sector -= 16;
    }
    return pImg->ReadTrackSectorSwapped(track, sector, buf, imageOrder,
            DiskImg::kSectorOrderDOS);
}

/*
 * Test for presence of 400K DOS 3.3 volumes.
 */
static DIError TestImageHalf(DiskImg* pImg, int trackOffset,
    DiskImg::SectorOrder imageOrder, int* pGoodCount)
{
    DIError dierr = kDIErrNone;
    uint8_t sctBuf[kSctSize];
    int numTracks, numSectors;
    int catTrack, catSect;
    int foundGood = 0;
    int iterations = 0;

    *pGoodCount = 0;

    dierr = ReadTrackSectorAdjusted(pImg, kVTOCTrack, kVTOCSector,
                trackOffset, sctBuf, imageOrder);
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
        LOGI("  UNI/Wide DOS header test failed");
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
        dierr = ReadTrackSectorAdjusted(pImg, catTrack, catSect,
                    trackOffset, sctBuf, imageOrder);
        if (dierr != kDIErrNone) {
            dierr = kDIErrNone;
            break;      /* allow it if earlier stuff was okay */
        }

        if (catTrack == sctBuf[1] && catSect == sctBuf[2] +1)
            foundGood++;
        else if (catTrack == sctBuf[1] && catSect == sctBuf[2]) {
            LOGI(" WideDOS detected self-reference on cat (%d,%d)",
                catTrack, catSect);
            break;
        }

        catTrack = sctBuf[1];
        catSect = sctBuf[2];
        iterations++;       // watch for infinite loops
    }
    if (iterations >= DiskFSDOS33::kMaxCatalogSectors) {
        dierr = kDIErrDirectoryLoop;
        LOGI("  WideDOS directory links cause a loop");
        goto bail;
    }

    LOGI(" WideDOS   foundGood=%d off=%d swap=%d", foundGood,
        trackOffset, imageOrder);
    *pGoodCount = foundGood;

bail:
    return dierr;
}

/*
 * Test both of the DOS partitions.
 */
static DIError TestImage(DiskImg* pImg, DiskImg::SectorOrder imageOrder,
    int* pGoodCount)
{
    DIError dierr;
    int goodCount1, goodCount2;

    *pGoodCount = 0;

    LOGI(" UNIDOS checking first half (imageOrder=%d)", imageOrder);
    dierr = TestImageHalf(pImg, 0, imageOrder, &goodCount1);
    if (dierr != kDIErrNone)
        return dierr;

    LOGI(" UNIDOS checking second half (imageOrder=%d)", imageOrder);
    dierr = TestImageHalf(pImg, kExpectedTracks, imageOrder, &goodCount2);
    if (dierr != kDIErrNone)
        return dierr;

    if (goodCount1 > goodCount2)
        *pGoodCount = goodCount1;
    else
        *pGoodCount = goodCount2;

    return kDIErrNone;
}

/*
 * Test to see if the image is a UNIDOS volume.
 */
/*static*/ DIError DiskFSUNIDOS::TestFS(DiskImg* pImg, DiskImg::SectorOrder* pOrder,
    DiskImg::FSFormat* pFormat, FSLeniency leniency)
{
    /* only on 800K disks (at the least, insist on numTracks being even) */
    if (pImg->GetNumBlocks() != kExpectedNumBlocks)
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
        LOGI(" WideDOS test: bestCount=%d for order=%d", bestCount, bestOrder);
        assert(bestOrder != DiskImg::kSectorOrderUnknown);
        *pOrder = bestOrder;
        *pFormat = DiskImg::kFormatUNIDOS;
        return kDIErrNone;
    }

    LOGI(" UNIDOS didn't find valid FS");
    return kDIErrFilesystemNotFound;
}

/*
 * Test to see if the image is a 'wide' (32-sector) DOS3.3 volume, i.e.
 * half of a UNIDOS volume (usually found embedded in ProDOS).
 *
 * Trying all possible formats is important here, because the wrong value for
 * swap can return a "good" value of 7 (much less than the expected 30, but
 * above a threshold of reasonableness).
 */
/*static*/ DIError DiskFSUNIDOS::TestWideFS(DiskImg* pImg, DiskImg::SectorOrder* pOrder,
    DiskImg::FSFormat* pFormat, FSLeniency leniency)
{
    /* only on 400K "disks" */
    if (pImg->GetNumBlocks() != kExpectedNumBlocks/2) {
        LOGI("  WideDOS ignoring volume (numBlocks=%ld)",
            pImg->GetNumBlocks());
        return kDIErrFilesystemNotFound;
    }

    DiskImg::SectorOrder ordering[DiskImg::kSectorOrderMax];
    
    DiskImg::GetSectorOrderArray(ordering, *pOrder);

    DiskImg::SectorOrder bestOrder = DiskImg::kSectorOrderUnknown;
    int bestCount = 0;

    for (int i = 0; i < DiskImg::kSectorOrderMax; i++) {
        int goodCount = 0;

        if (ordering[i] == DiskImg::kSectorOrderUnknown)
            continue;
        if (TestImageHalf(pImg, 0, ordering[i], &goodCount) == kDIErrNone) {
            if (goodCount > bestCount) {
                bestCount = goodCount;
                bestOrder = ordering[i];
            }
        }
    }

    if (bestCount >= 4 ||
        (leniency == kLeniencyVery && bestCount >= 2))
    {
        LOGI(" UNI/Wide test: bestCount=%d for order=%d", bestCount, bestOrder);
        assert(bestOrder != DiskImg::kSectorOrderUnknown);
        *pOrder = bestOrder;
        *pFormat = DiskImg::kFormatDOS33;
        // up to the caller to adjust numTracks/numSectPerTrack
        return kDIErrNone;
    }

    LOGI(" UNI/Wide didn't find valid FS");
    return kDIErrFilesystemNotFound;
}


/*
 * Set up our sub-volumes.
 */
DIError DiskFSUNIDOS::Initialize(void)
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
        LOGI(" UNIDOS not scanning for sub-volumes");
    }

    SetVolumeUsageMap();

    return kDIErrNone;
}

/*
 * Open up one of the DOS 3.3 sub-volumes.
 */
DIError DiskFSUNIDOS::OpenSubVolume(int idx)
{
    DIError dierr = kDIErrNone;
    DiskFS* pNewFS = NULL;
    DiskImg* pNewImg = NULL;
    
    pNewImg = new DiskImg;
    if (pNewImg == NULL) {
        dierr = kDIErrMalloc;
        goto bail;
    }

    dierr = pNewImg->OpenImage(fpImg, kExpectedTracks * idx, 0,
                kExpectedTracks * kExpectedSectors);
    if (dierr != kDIErrNone) {
        LOGI(" UNISub: OpenImage(%d,0,%d) failed (err=%d)",
            kExpectedTracks * idx, kExpectedTracks * kExpectedSectors, dierr);
        goto bail;
    }

    dierr = pNewImg->AnalyzeImage();
    if (dierr != kDIErrNone) {
        LOGI(" UNISub: analysis failed (err=%d)", dierr);
        goto bail;
    }

    if (pNewImg->GetFSFormat() == DiskImg::kFormatUnknown ||
        pNewImg->GetSectorOrder() == DiskImg::kSectorOrderUnknown)
    {
        LOGI(" UNISub: unable to identify filesystem");
        dierr = kDIErrUnsupportedFSFmt;
        goto bail;
    }

    /* open a DiskFS for the sub-image */
    LOGI(" UNISub %d succeeded!", idx);
    pNewFS = pNewImg->OpenAppropriateDiskFS();
    if (pNewFS == NULL) {
        LOGI(" UNISub: OpenAppropriateDiskFS failed");
        dierr = kDIErrUnsupportedFSFmt;
        goto bail;
    }

    /* load the files from the sub-image */
    dierr = pNewFS->Initialize(pNewImg, kInitFull);
    if (dierr != kDIErrNone) {
        LOGE(" UNISub: error %d reading list of files from disk", dierr);
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
