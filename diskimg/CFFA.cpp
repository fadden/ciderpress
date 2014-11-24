/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * The "CFFA" DiskFS is a container class for multiple ProDOS and HFS volumes.
 *
 * The CFFA card doesn't have any RAM, so the author used a fixed partitioning
 * scheme.  You get 4 or 8 volumes -- depending on which firmware you jumper
 * in -- at 32MB each.  CF cards usually hold less than you would expect, so
 * a 64MB card would have one 32MB volume and one less-than-32MB volume.
 *
 * With Dave's GS/OS driver, you get an extra drive or two at the end, at up
 * to 1GB each.  The driver only works in 4-volume mode.
 *
 * There is no magic CFFA block at the front, so it looks like a plain
 * ProDOS or HFS volume.  If the size is less than 32MB -- meaning there's
 * only one volume -- we don't need to take an interest in the file,
 * because the regular filesystem goodies will handle it just fine.  If it's
 * more than 32MB, we need to create a structure in which multiple volumes
 * reside.
 *
 * The trick is finding all the volumes.  The first four are easy.  The
 * fifth one is either another 32MB volume (if you're in 8-volume mode)
 * or a volume whose size is somewhere between the amount of space left
 * and 1GB.  Not an issue until we get to CF cards > 128MB.  We have to
 * rely on the CFFA card making volumes as large as it can.
 *
 * I think it's reasonable to require that the first volume be either ProDOS
 * or HFS.  That way we don't go digging through large non-CFFA files when
 * auto-probing.
 */
#include "StdAfx.h"
#include "DiskImgPriv.h"

/*
 * Figure out if this is a CFFA volume, and if so, whether it was formatted
 * in 4-partition or 8-partition mode.
 *
 * The "imageOrder" parameter has no use here, because (in the current
 * version) embedded parent volumes are implicitly ProDOS-ordered.
 *
 * "*pFormatFound" should be either a CFFA format or "unknown" on entry.
 * If it's not "unknown", we will look for the specified format first.
 * Otherwise, we look for 4-partition then 8-partition.  The first one
 * we find successfully is returned.
 *
 * Ideally we'd have some way to express ambiguity here, so that we could
 * force the "disk format verification" dialog to come up.  No such
 * mechanism exists, and for now it doesn't seem worthwhile to add one.
 */
/*static*/ DIError DiskFSCFFA::TestImage(DiskImg* pImg,
    DiskImg::SectorOrder imageOrder, DiskImg::FSFormat* pFormatFound)
{
    DIError dierr;
    long totalBlocks = pImg->GetNumBlocks();
    long startBlock, maxBlocks, totalBlocksLeft;
    long fsNumBlocks;
    DiskFS* pNewFS = NULL;
    DiskImg* pNewImg = NULL;
    //bool fiveIs32MB;

    assert(totalBlocks > kEarlyVolExpectedSize);

    // could be "generic" from an earlier override
    if (*pFormatFound != DiskImg::kFormatCFFA4 &&
        *pFormatFound != DiskImg::kFormatCFFA8)
    {
        *pFormatFound = DiskImg::kFormatUnknown;
    }

    LOGI("----- BEGIN CFFA SCAN (fmt=%d) -----", *pFormatFound);

    startBlock = 0;
    totalBlocksLeft = totalBlocks;

    /*
     * Look for a 32MB ProDOS or HFS volume in the first slot.  If we
     * don't find this, it's probably not CFFA.  It's possible they just
     * didn't format the first one, but that seems unlikely, and it's not
     * unreasonable to insist that they format the first partition.
     */
    maxBlocks = totalBlocksLeft;
    if (maxBlocks > kEarlyVolExpectedSize)
        maxBlocks = kEarlyVolExpectedSize;

    dierr = OpenSubVolume(pImg, startBlock, maxBlocks, true,
                &pNewImg, &pNewFS);
    if (dierr != kDIErrNone) {
        LOGI(" CFFA failed opening sub-volume #1");
        goto bail;
    }
    fsNumBlocks = pNewFS->GetFSNumBlocks();
    delete pNewFS;
    delete pNewImg;
    if (fsNumBlocks != kEarlyVolExpectedSize &&
        fsNumBlocks != kEarlyVolExpectedSize-1)
    {
        LOGI("  CFFA found fsNumBlocks=%ld in slot #1", fsNumBlocks);
        dierr = kDIErrFilesystemNotFound;
        goto bail;
    }
    LOGI("  CFFA found good volume in slot #1");

    startBlock += maxBlocks;
    totalBlocksLeft -= maxBlocks;
    assert(totalBlocksLeft > 0);

    /*
     * Look for a ProDOS or HFS volume <= 32MB in the second slot.  If
     * we don't find something here, and this is a 64MB card, then there's
     * no advantage to using CFFA (in fact, the single-volume handling may
     * be more convenient).  If there's at least another 32MB, we continue
     * looking.
     */
    maxBlocks = totalBlocksLeft;
    if (maxBlocks > kEarlyVolExpectedSize)
        maxBlocks = kEarlyVolExpectedSize;

    dierr = OpenSubVolume(pImg, startBlock, maxBlocks, true,
                &pNewImg, &pNewFS);
    if (dierr != kDIErrNone) {
        LOGI(" CFFA failed opening sub-volume #2");
        if (maxBlocks < kEarlyVolExpectedSize)
            goto bail;
        // otherwise, assume they just didn't format #2, and keep going
    } else {
        fsNumBlocks = pNewFS->GetFSNumBlocks();
        delete pNewFS;
        delete pNewImg;
#if 0
        if (fsNumBlocks != kEarlyVolExpectedSize &&
            fsNumBlocks != kEarlyVolExpectedSize-1)
        {
            LOGI("  CFFA found fsNumBlocks=%ld in slot #2", fsNumBlocks);
            dierr = kDIErrFilesystemNotFound;
            goto bail;
        }
#endif
        LOGI("  CFFA found good volume in slot #2");
    }

    startBlock += maxBlocks;
    totalBlocksLeft -= maxBlocks;
    if (totalBlocksLeft == 0) {
        *pFormatFound = DiskImg::kFormatCFFA4;
        goto bail;
    }

    /*
     * Skip #3 and #4.
     */
    LOGI("  CFFA skipping over slot #3");
    maxBlocks = kEarlyVolExpectedSize*2;
    if (maxBlocks > totalBlocksLeft)
        maxBlocks = totalBlocksLeft;
    startBlock += maxBlocks;
    totalBlocksLeft -= maxBlocks;
    if (totalBlocksLeft == 0) {
        // no more partitions to find; we're done
        *pFormatFound = DiskImg::kFormatCFFA4;
        goto bail;
    }
    LOGI("  CFFA skipping over slot #4");

    /*
     * Partition #5.  We know where it starts, but not how large it is.
     * Could be 32MB, could be 1GB, could be anything between.
     *
     * CF cards come in power-of-two sizes -- 128MB, 256MB, etc. -- but
     * we don't want to make assumptions here.  It's possible we're
     * looking at an odd-sized image file that some clever person is
     * expecting to access with CiderPress.
     */
    maxBlocks = totalBlocksLeft;
    if (maxBlocks > kOneGB)
        maxBlocks = kOneGB;
    if (maxBlocks <= kEarlyVolExpectedSize) {
        /*
         * Only enough room for one <= 32MB volume.  Not expected for a
         * real CFFA card, unless they come in 160MB sizes.
         *
         * Treat it like 4-partition; it'll look like somebody slapped a
         * 32MB volume into the first 1GB area.
         */
        LOGI("  CFFA assuming odd-sized slot #5");
        *pFormatFound = DiskImg::kFormatCFFA4;
        goto bail;
    }

    /*
     * We could be looking at a 32MB ProDOS partition, 32MB HFS partition,
     * or an up to 1GB HFS partition.  We have to specify the size in
     * the OpenSubVolume request, which means trying it both ways and
     * finding a match from GetFSNumBlocks().  Complicating matters is
     * truncation (ProDOS max 65535, not 65536) and round-off (not sure
     * how HFS deals with left-over blocks).
     *
     * Start with <= 1GB.
     */
    dierr = OpenSubVolume(pImg, startBlock, maxBlocks, true,
                &pNewImg, &pNewFS);
    if (dierr != kDIErrNone) {
        if (dierr == kDIErrCancelled)
            goto bail;
        LOGI("  CFFA failed opening large sub-volume #5");
        // if we can't get #5, don't bother looking for #6
        // (we could try anyway, but it's easier to just skip it)
        dierr = kDIErrNone;
        *pFormatFound = DiskImg::kFormatCFFA4;
        goto bail;
    }

    fsNumBlocks = pNewFS->GetFSNumBlocks();
    delete pNewFS;
    delete pNewImg;
    if (fsNumBlocks < 2 || fsNumBlocks > maxBlocks) {
        LOGI("   CFFA WARNING: FSNumBlocks #5 reported blocks=%ld",
            fsNumBlocks);
    }
    if (fsNumBlocks == kEarlyVolExpectedSize-1 ||
        fsNumBlocks == kEarlyVolExpectedSize)
    {
        LOGI("  CFFA #5 is a 32MB volume");
        // tells us nothing -- could still be 4 or 8, so we keep going
        maxBlocks = kEarlyVolExpectedSize;
    } else if (fsNumBlocks > kEarlyVolExpectedSize) {
        // must be a GS/OS 1GB area
        LOGI("  CFFA #5 is larger than 32MB");
        *pFormatFound = DiskImg::kFormatCFFA4;
        goto bail;
    } else {
        LOGI("  CFFA #5 was unexpectedly small (%ld blocks)", fsNumBlocks);
        // just stop now
        *pFormatFound = DiskImg::kFormatCFFA4;
        goto bail;
    }

    startBlock += maxBlocks;
    totalBlocksLeft -= maxBlocks;

    if (!totalBlocksLeft) {
        LOGI(" CFFA got 5 volumes");
        *pFormatFound = DiskImg::kFormatCFFA4;
        goto bail;
    }

    /*
     * Various possibilities for slots 5 and up:
     *  A. Card in 4-partition mode.  5th partition isn't formatted.  Don't
     *     bother looking for 6th.  [already handled]
     *  B. Card in 4-partition mode.  5th partition is >32MB HFS.  6th
     *     partition will be at +1GB.  [already handled]
     *  C. Card in 4-partition mode.  5th partition is 32MB ProDOS.  6th
     *     partition will be at +1GB.
     *  D. Card in 8-partition mode.  5th partition is 32MB HFS.  6th
     *     partition will be at +32MB.
     *  E. Card in 8-partition mode.  5th partition is 32MB ProDOS.  6th
     *     partition will be at +32MB.
     *
     * I'm ignoring D on the off chance somebody could create a 32MB HFS
     * partition in the 1GB space.  D and E are handled alike.
     *
     * The difference between C and D/E can *usually* be determined by
     * opening up a 6th partition in the two expected locations.
     */
    LOGI(" CFFA probing 6th slot for 4-vs-8");
    /*
     * Look in two different places.  If we find something at the
     * +32MB mark, assume it's in "8 mode".  If we find something at
     * the +1GB mark, assume it's in "4 + GS/OS mode".  If both exist
     * we have a problem.
     */
    bool foundSmall, foundGig;

    foundSmall = false;
    maxBlocks = totalBlocksLeft;
    if (maxBlocks > kEarlyVolExpectedSize)
        maxBlocks = kEarlyVolExpectedSize;
    dierr = OpenSubVolume(pImg, startBlock + kEarlyVolExpectedSize,
                maxBlocks, true, &pNewImg, &pNewFS);
    if (dierr != kDIErrNone) {
        if (dierr == kDIErrCancelled)
            goto bail;
        LOGI(" CFFA no vol #6 found at +32MB");
    } else {
        foundSmall = true;
        delete pNewFS;
        delete pNewImg;
    }

    foundGig = false;
    // no need to look if we don't have at least 1GB left!
    if (totalBlocksLeft >= kOneGB) {
        maxBlocks = totalBlocksLeft;
        if (maxBlocks > kOneGB)
            maxBlocks = kOneGB;
        dierr = OpenSubVolume(pImg, startBlock + kOneGB,
                    maxBlocks, true, &pNewImg, &pNewFS);
        if (dierr != kDIErrNone) {
            if (dierr == kDIErrCancelled)
                goto bail;
            LOGI(" CFFA no vol #6 found at +1GB");
        } else {
            foundGig = true;
            delete pNewFS;
            delete pNewImg;
        }
    }

    dierr = kDIErrNone;

    if (!foundSmall && !foundGig) {
        LOGI(" CFFA no valid filesystem found in 6th position");
        *pFormatFound = DiskImg::kFormatCFFA4;
        // don't bother looking for 7 and 8
    } else if (foundSmall && foundGig) {
        LOGI(" CFFA WARNING: found valid volumes at +32MB *and* +1GB");
        // default to 4-partition mode
        if (*pFormatFound == DiskImg::kFormatUnknown)
            *pFormatFound = DiskImg::kFormatCFFA4;
    } else if (foundGig) {
        LOGI(" CFFA found 6th volume at +1GB, assuming 4-mode w/GSOS");
        if (fsNumBlocks < 2 || fsNumBlocks > kOneGB) {
            LOGI(" CFFA WARNING: FSNumBlocks #6 reported as %ld",
                fsNumBlocks);
        }
        *pFormatFound = DiskImg::kFormatCFFA4;
    } else if (foundSmall) {
        LOGI(" CFFA found  6th volume at +32MB, assuming 8-mode");
        if (fsNumBlocks < 2 || fsNumBlocks > kEarlyVolExpectedSize) {
            LOGI(" CFFA WARNING: FSNumBlocks #6 reported as %ld",
                fsNumBlocks);
        }
        *pFormatFound = DiskImg::kFormatCFFA8;
    } else {
        assert(false);      // how'd we get here??
    }

    // done!

bail:
    LOGI("----- END CFFA SCAN (err=%d format=%d) -----",
        dierr, *pFormatFound);
    if (dierr == kDIErrNone) {
        assert(*pFormatFound != DiskImg::kFormatUnknown);
    } else {
        *pFormatFound = DiskImg::kFormatUnknown;
    }
    return dierr;
}


/*
 * Open up a sub-volume.
 *
 * If "scanOnly" is set, the full DiskFS initialization isn't performed.
 * We just do enough to get the volume size info.
 */
/*static*/ DIError DiskFSCFFA::OpenSubVolume(DiskImg* pImg, long startBlock,
    long numBlocks, bool scanOnly, DiskImg** ppNewImg, DiskFS** ppNewFS)
{
    DIError dierr = kDIErrNone;
    DiskFS* pNewFS = NULL;
    DiskImg* pNewImg = NULL;
    
    pNewImg = new DiskImg;
    if (pNewImg == NULL) {
        dierr = kDIErrMalloc;
        goto bail;
    }

    dierr = pNewImg->OpenImage(pImg, startBlock, numBlocks);
    if (dierr != kDIErrNone) {
        LOGI(" CFFASub: OpenImage(%ld,%ld) failed (err=%d)",
            startBlock, numBlocks, dierr);
        goto bail;
    }
    //LOGI("  +++ CFFASub: new image has ro=%d (parent=%d)",
    //  pNewImg->GetReadOnly(), pImg->GetReadOnly());

    dierr = pNewImg->AnalyzeImage();
    if (dierr != kDIErrNone) {
        LOGI(" CFFASub: analysis failed (err=%d)", dierr);
        goto bail;
    }

    if (pNewImg->GetFSFormat() == DiskImg::kFormatUnknown ||
        pNewImg->GetSectorOrder() == DiskImg::kSectorOrderUnknown)
    {
        LOGI(" CFFASub: unable to identify filesystem at %ld",
            startBlock);
        dierr = kDIErrUnsupportedFSFmt;
        goto bail;
    }

    /* open a DiskFS for the sub-image */
    LOGI(" CFFASub (%ld,%ld) analyze succeeded!", startBlock, numBlocks);
    pNewFS = pNewImg->OpenAppropriateDiskFS();
    if (pNewFS == NULL) {
        LOGI(" CFFASub: OpenAppropriateDiskFS failed");
        dierr = kDIErrUnsupportedFSFmt;
        goto bail;
    }

    /* we encapsulate arbitrary stuff, so encourage child to scan */
    pNewFS->SetScanForSubVolumes(kScanSubEnabled);

    /*
     * Load the files from the sub-image.  When doing our initial tests,
     * or when loading data for the volume copier, we don't want to dig
     * into our sub-volumes, just figure out what they are and where.
     */
    InitMode initMode;
    if (scanOnly)
        initMode = kInitHeaderOnly;
    else
        initMode = kInitFull;
    dierr = pNewFS->Initialize(pNewImg, initMode);
    if (dierr != kDIErrNone) {
        LOGI(" CFFASub: error %d reading list of files from disk", dierr);
        goto bail;
    }

bail:
    if (dierr != kDIErrNone) {
        delete pNewFS;
        delete pNewImg;
    } else {
        *ppNewImg = pNewImg;
        *ppNewFS = pNewFS;
    }
    return dierr;
}

/*
 * Check to see if this is a CFFA volume.
 */
/*static*/ DIError DiskFSCFFA::TestFS(DiskImg* pImg, DiskImg::SectorOrder* pOrder,
    DiskImg::FSFormat* pFormat, FSLeniency leniency)
{
    if (pImg->GetNumBlocks() < kMinInterestingBlocks)
        return kDIErrFilesystemNotFound;
    if (pImg->GetIsEmbedded())      // don't look for CFFA inside CFFA!
        return kDIErrFilesystemNotFound;

    /* assume ProDOS -- shouldn't matter, since it's embedded */
    if (TestImage(pImg, DiskImg::kSectorOrderProDOS, pFormat) == kDIErrNone) {
        assert(*pFormat == DiskImg::kFormatCFFA4 ||
               *pFormat == DiskImg::kFormatCFFA8);
        *pOrder = DiskImg::kSectorOrderProDOS;
        return kDIErrNone;
    }

    // make sure we didn't tamper with it
    assert(*pFormat == DiskImg::kFormatUnknown);

    LOGI("  FS didn't find valid CFFA");
    return kDIErrFilesystemNotFound;
}


/*
 * Prep the CFFA "container" for use.
 */
DIError DiskFSCFFA::Initialize(void)
{
    DIError dierr = kDIErrNone;

    LOGI("CFFA initializing (scanForSub=%d)", fScanForSubVolumes);

    /* seems pointless *not* to, but we just do what we're told */
    if (fScanForSubVolumes != kScanSubDisabled) {
        dierr = FindSubVolumes();
        if (dierr != kDIErrNone)
            return dierr;
    }

    /* blank out the volume usage map */
    SetVolumeUsageMap();

    return dierr;
}


/*
 * Find the various sub-volumes and open them.
 *
 * We don't handle the volume specially unless it's at least 32MB, which
 * means there are at least 2 partitions.
 */
DIError DiskFSCFFA::FindSubVolumes(void)
{
    DIError dierr;
    long startBlock, blocksLeft;

    startBlock = 0;
    blocksLeft = fpImg->GetNumBlocks();

    if (fpImg->GetFSFormat() == DiskImg::kFormatCFFA4) {
        LOGI(" CFFA opening 4+2 volumes");
        dierr = AddVolumeSeries(0, 4, kEarlyVolExpectedSize, /*ref*/startBlock,
                /*ref*/blocksLeft);
        if (dierr != kDIErrNone)
            goto bail;

        LOGI(" CFFA after first 4, startBlock=%ld blocksLeft=%ld",
            startBlock, blocksLeft);
        if (blocksLeft > 0) {
            dierr = AddVolumeSeries(4, 2, kOneGB, /*ref*/startBlock,
                    /*ref*/blocksLeft);
            if (dierr != kDIErrNone)
                goto bail;
        }
    } else if (fpImg->GetFSFormat() == DiskImg::kFormatCFFA8) {
        LOGI(" CFFA opening 8 volumes");
        dierr = AddVolumeSeries(0, 8, kEarlyVolExpectedSize, /*ref*/startBlock,
            /*ref*/blocksLeft);
        if (dierr != kDIErrNone)
            goto bail;
    } else {
        assert(false);
        return kDIErrInternal;
    }

    if (blocksLeft != 0) {
        LOGI("  CFFA ignoring leftover %ld blocks", blocksLeft);
    }

bail:
    return dierr;
}

/*
 * Add a series of equal-sized volumes.
 *
 * Updates "startBlock" and "totalBlocksLeft".
 */
DIError DiskFSCFFA::AddVolumeSeries(int start, int count, long blocksPerVolume,
    long& startBlock, long& totalBlocksLeft)
{
    DIError dierr = kDIErrNone;
    DiskFS* pNewFS = NULL;
    DiskImg* pNewImg = NULL;
    long maxBlocks, fsNumBlocks;
    bool scanOnly = false;

    /* used by volume copier, to avoid deep scan */
    if (GetScanForSubVolumes() == kScanSubContainerOnly)
        scanOnly = true;

    for (int i = start; i < start+count; i++) {
        maxBlocks = blocksPerVolume;
        if (maxBlocks > totalBlocksLeft)
            maxBlocks = totalBlocksLeft;

        dierr = OpenSubVolume(fpImg, startBlock, maxBlocks, scanOnly,
                    &pNewImg, &pNewFS);
        if (dierr != kDIErrNone) {
            if (dierr == kDIErrCancelled)
                goto bail;
            LOGI(" CFFA failed opening sub-volume %d (not formatted?)", i);
            /* create a fake one to represent the partition */
            dierr = CreatePlaceholder(startBlock, maxBlocks, NULL, NULL,
                        &pNewImg, &pNewFS);
            if (dierr == kDIErrNone) {
                AddSubVolumeToList(pNewImg, pNewFS);
            } else {
                LOGI("  CFFA unable to create placeholder (%ld, %ld) (err=%d)",
                    startBlock, maxBlocks, dierr);
                goto bail;
            }
        } else {
            fsNumBlocks = pNewFS->GetFSNumBlocks();
            if (fsNumBlocks < 2 || fsNumBlocks > blocksPerVolume) {
                LOGI(" CFFA WARNING: FSNumBlocks #%d reported as %ld",
                    i, fsNumBlocks);
            }
            AddSubVolumeToList(pNewImg, pNewFS);
        }

        startBlock += maxBlocks;
        totalBlocksLeft -= maxBlocks;
        if (!totalBlocksLeft)
            break;          // all done
    }

bail:
    return dierr;
}
