/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * The "MicroDrive" DiskFS is a container class for multiple ProDOS and HFS
 * volumes.  It represents a partitioned disk device, such as a hard
 * drive or CF card, that has been formatted for use with ///SHH Systeme's
 * MicroDrive card for the Apple II.
 */
#include "StdAfx.h"
#include "DiskImgPriv.h"


const int kBlkSize = 512;
const int kPartMapBlock = 0;    // partition map lives here
const uint32_t kPartSizeMask = 0x00ffffff;


/*
 * Format of partition map.  It resides in the first 256 bytes of block 0.
 * All values are in little-endian order.
 *
 * The layout was discovered through reverse-engineering.  Additional notes:
 *
    From Joachim Lange:

    Below, this is the configuration block as it is used in all
    MicroDrive cards. Please verify that my ID shortcut can be
    found at offset 0, otherwise the partition info is not
    valid. Most of the other parms are not useful, some are
    historic and not useful anymore. As a second security
    measure, verify that the first partition starts at
    absolute block 256. This is also a fixed value used in all
    MicroDrive cards. Of course the partition size is not two
    bytes long but three (not four), the 4th byte is used for
    switching drives in a two-drive configuration. So, for
    completeness, when reading partition sizes, perform a
    partitionLength[..] & 0x00FFFFFF, or at least issue a
    warning that something may be wrong. The offset
    (partitionStart) could reach into the 4th byte.
    I have attached the config block in a zip file because
    the mailer would probably re-format the source text.
 */
const int kMaxNumParts = 8;
typedef struct DiskFSMicroDrive::PartitionMap {
    uint16_t    magic;              // partition signature
    uint16_t    cylinders;          // #of cylinders
    uint16_t    reserved1;          // ??
    uint16_t    heads;              // #of heads/cylinder
    uint16_t    sectors;            // #of sectors/track
    uint16_t    reserved2;          // ??
    uint8_t     numPart1;           // #of partitions in first chunk
    uint8_t     numPart2;           // #of partitions in second chunk
    uint8_t     reserved3[10];      // bytes 0x0e-0x17
    uint16_t    romVersion;         // IIgs ROM01 or ROM03
    uint8_t     reserved4[6];       // bytes 0x1a-0x1f
    uint32_t    partitionStart1[kMaxNumParts];  // bytes 0x20-0x3f
    uint32_t    partitionLength1[kMaxNumParts]; // bytes 0x40-0x5f
    uint8_t     reserved5[32];      // bytes 0x60-0x7f
    uint32_t    partitionStart2[kMaxNumParts];  // bytes 0x80-0x9f
    uint32_t    partitionLength2[kMaxNumParts]; // bytes 0xa0-0xbf

    uint8_t     padding[320];
} PartitionMap;


/*
 * Figure out if this is a MicroDrive partition.
 *
 * The "imageOrder" parameter has no use here, because (in the current
 * version) embedded parent volumes are implicitly ProDOS-ordered.
 */
/*static*/ DIError DiskFSMicroDrive::TestImage(DiskImg* pImg,
    DiskImg::SectorOrder imageOrder)
{
    DIError dierr = kDIErrNone;
    uint8_t blkBuf[kBlkSize];
    int partCount1, partCount2;

    assert(sizeof(PartitionMap) == kBlkSize);

    /*
     * See if block 0 is a MicroDrive partition map.
     */
    dierr = pImg->ReadBlockSwapped(kPartMapBlock, blkBuf, imageOrder,
                DiskImg::kSectorOrderProDOS);
    if (dierr != kDIErrNone)
        goto bail;

    if (GetShortLE(&blkBuf[0x00]) != kPartitionSignature) {
        LOGI(" MicroDrive partition signature not found in first part block");
        dierr = kDIErrFilesystemNotFound;
        goto bail;
    }
    /* might assert that partCount2 be zero unless partCount1 == 8? */
    partCount1 = blkBuf[0x0c];
    partCount2 = blkBuf[0x0d];
    if (partCount1 == 0 || partCount1 > kMaxNumParts ||
        partCount2 > kMaxNumParts)
    {
        LOGI(" MicroDrive unreasonable partCount values %d/%d",
            partCount1, partCount2);
        dierr = kDIErrFilesystemNotFound;
        goto bail;
    }

    /* consider testing other fields */

    // success!
    LOGI(" MicroDrive partition map count = %d/%d", partCount1, partCount2);

bail:
    return dierr;
}


/*
 * Unpack a partition map block into a partition map data structure.
 */
/*static*/ void DiskFSMicroDrive::UnpackPartitionMap(const uint8_t* buf,
    PartitionMap* pMap)
{
    pMap->magic = GetShortLE(&buf[0x00]);
    pMap->cylinders = GetShortLE(&buf[0x02]);
    pMap->reserved1 = GetShortLE(&buf[0x04]);
    pMap->heads = GetShortLE(&buf[0x06]);
    pMap->sectors = GetShortLE(&buf[0x08]);
    pMap->reserved2 = GetShortLE(&buf[0x0a]);
    pMap->numPart1 = buf[0x0c];
    pMap->numPart2 = buf[0x0d];
    memcpy(pMap->reserved3, &buf[0x0e], sizeof(pMap->reserved3));
    pMap->romVersion = GetShortLE(&buf[0x18]);
    memcpy(pMap->reserved4, &buf[0x1a], sizeof(pMap->reserved4));

    for (int i = 0; i < kMaxNumParts; i++) {
        pMap->partitionStart1[i] = GetLongLE(&buf[0x20] + i * 4);
        pMap->partitionLength1[i] = GetLongLE(&buf[0x40] + i * 4) & kPartSizeMask;
        pMap->partitionStart2[i] = GetLongLE(&buf[0x80] + i * 4);
        pMap->partitionLength2[i] = GetLongLE(&buf[0xa0] + i * 4) & kPartSizeMask;
    }
    memcpy(pMap->reserved5, &buf[0x60], sizeof(pMap->reserved5));
    memcpy(pMap->padding, &buf[0x80], sizeof(pMap->padding));
}

/*
 * Debug: dump the contents of the partition map.
 */
/*static*/ void DiskFSMicroDrive::DumpPartitionMap(const PartitionMap* pMap)
{
    LOGI(" MicroDrive partition map:");
    LOGI("    cyls=%d res1=%d heads=%d sects=%d",
        pMap->cylinders, pMap->reserved1, pMap->heads, pMap->sectors);
    LOGI("    res2=%d numPart1=%d numPart2=%d",
        pMap->reserved2, pMap->numPart1, pMap->numPart2);
    LOGI("    romVersion=ROM%02d", pMap->romVersion);

    int i, parts;

    parts = pMap->numPart1;
    assert(parts <= kMaxNumParts);
    for (i = 0; i < parts; i++) {
        LOGI("    %2d: startLBA=%8d length=%d",
            i, pMap->partitionStart1[i], pMap->partitionLength1[i]);
    }
    parts = pMap->numPart2;
    assert(parts <= kMaxNumParts);
    for (i = 0; i < parts; i++) {
        LOGI("    %2d: startLBA=%8d length=%d",
            i+8, pMap->partitionStart2[i], pMap->partitionLength2[i]);
    }
}


/*
 * Open up a sub-volume.
 */
DIError DiskFSMicroDrive::OpenSubVolume(long startBlock, long numBlocks)
{
    DIError dierr = kDIErrNone;
    DiskFS* pNewFS = NULL;
    DiskImg* pNewImg = NULL;
    //bool tweaked = false;

    LOGI("Adding %ld +%ld", startBlock, numBlocks);

    if (startBlock > fpImg->GetNumBlocks()) {
        LOGI("MicroDrive start block out of range (%ld vs %ld)",
            startBlock, fpImg->GetNumBlocks());
        return kDIErrBadPartition;
    }
    if (startBlock + numBlocks > fpImg->GetNumBlocks()) {
        LOGI("MicroDrive partition too large (%ld vs %ld avail)",
            numBlocks, fpImg->GetNumBlocks() - startBlock);
        fpImg->AddNote(DiskImg::kNoteInfo,
            "Reduced partition from %ld blocks to %ld.\n",
            numBlocks, fpImg->GetNumBlocks() - startBlock);
        numBlocks = fpImg->GetNumBlocks() - startBlock;
        //tweaked = true;
    }

    pNewImg = new DiskImg;
    if (pNewImg == NULL) {
        dierr = kDIErrMalloc;
        goto bail;
    }

    dierr = pNewImg->OpenImage(fpImg, startBlock, numBlocks);
    if (dierr != kDIErrNone) {
        LOGI(" MicroDriveSub: OpenImage(%ld,%ld) failed (err=%d)",
            startBlock, numBlocks, dierr);
        goto bail;
    }

    //LOGI("  +++ CFFASub: new image has ro=%d (parent=%d)",
    //  pNewImg->GetReadOnly(), pImg->GetReadOnly());

    /* figure out what the format is */
    dierr = pNewImg->AnalyzeImage();
    if (dierr != kDIErrNone) {
        LOGI(" MicroDriveSub: analysis failed (err=%d)", dierr);
        goto bail;
    }

    /* we allow unrecognized partitions */
    if (pNewImg->GetFSFormat() == DiskImg::kFormatUnknown ||
        pNewImg->GetSectorOrder() == DiskImg::kSectorOrderUnknown)
    {
        LOGI(" MicroDriveSub (%ld,%ld): unable to identify filesystem",
            startBlock, numBlocks);
        DiskFSUnknown* pUnknownFS = new DiskFSUnknown;
        if (pUnknownFS == NULL) {
            dierr = kDIErrInternal;
            goto bail;
        }
        //pUnknownFS->SetVolumeInfo((const char*)pMap->pmParType);
        pNewFS = pUnknownFS;
        //pNewImg->AddNote(DiskImg::kNoteInfo, "Partition name='%s' type='%s'.",
        //  pMap->pmPartName, pMap->pmParType);
    } else {
        /* open a DiskFS for the sub-image */
        LOGI(" MicroDriveSub (%ld,%ld) analyze succeeded!", startBlock, numBlocks);
        pNewFS = pNewImg->OpenAppropriateDiskFS(true);
        if (pNewFS == NULL) {
            LOGI(" MicroDriveSub: OpenAppropriateDiskFS failed");
            dierr = kDIErrUnsupportedFSFmt;
            goto bail;
        }
    }

    /* we encapsulate arbitrary stuff, so encourage child to scan */
    pNewFS->SetScanForSubVolumes(kScanSubEnabled);

    /*
     * Load the files from the sub-image.  When doing our initial tests,
     * or when loading data for the volume copier, we don't want to dig
     * into our sub-volumes, just figure out what they are and where.
     *
     * If "initialize" fails, the sub-volume won't get added to the list.
     * It's important that a failure at this stage doesn't cause the whole
     * thing to fall over.
     */
    InitMode initMode;
    if (GetScanForSubVolumes() == kScanSubContainerOnly)
        initMode = kInitHeaderOnly;
    else
        initMode = kInitFull;
    dierr = pNewFS->Initialize(pNewImg, initMode);
    if (dierr != kDIErrNone) {
        LOGI(" MicroDriveSub: error %d reading list of files from disk", dierr);
        goto bail;
    }

    /* add it to the list */
    AddSubVolumeToList(pNewImg, pNewFS);
    pNewImg = NULL;
    pNewFS = NULL;

bail:
    delete pNewFS;
    delete pNewImg;
    return dierr;
}

/*
 * Check to see if this is a MicroDrive volume.
 */
/*static*/ DIError DiskFSMicroDrive::TestFS(DiskImg* pImg, DiskImg::SectorOrder* pOrder,
    DiskImg::FSFormat* pFormat, FSLeniency leniency)
{
    if (pImg->GetNumBlocks() < kMinInterestingBlocks)
        return kDIErrFilesystemNotFound;
    if (pImg->GetIsEmbedded())      // don't look for partitions inside
        return kDIErrFilesystemNotFound;

    /* assume ProDOS -- shouldn't matter, since it's embedded */
    if (TestImage(pImg, DiskImg::kSectorOrderProDOS) == kDIErrNone) {
        *pFormat = DiskImg::kFormatMicroDrive;
        *pOrder = DiskImg::kSectorOrderProDOS;
        return kDIErrNone;
    }

    LOGI("  FS didn't find valid MicroDrive");
    return kDIErrFilesystemNotFound;
}


/*
 * Prep the MicroDrive "container" for use.
 */
DIError DiskFSMicroDrive::Initialize(void)
{
    DIError dierr = kDIErrNone;

    LOGI("MicroDrive initializing (scanForSub=%d)", fScanForSubVolumes);

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
 */
DIError DiskFSMicroDrive::FindSubVolumes(void)
{
    DIError dierr = kDIErrNone;
    uint8_t buf[kBlkSize];
    PartitionMap map;
    int i;

    dierr = fpImg->ReadBlock(kPartMapBlock, buf);
    if (dierr != kDIErrNone)
        goto bail;
    UnpackPartitionMap(buf, &map);
    DumpPartitionMap(&map);

    /* first part of the table */
    for (i = 0; i < map.numPart1; i++) {
        dierr = OpenVol(i,
                            map.partitionStart1[i], map.partitionLength1[i]);
        if (dierr != kDIErrNone)
            goto bail;
    }

    /* second part of the table */
    for (i = 0; i < map.numPart2; i++) {
        dierr = OpenVol(i + kMaxNumParts,
                            map.partitionStart2[i], map.partitionLength2[i]);
        if (dierr != kDIErrNone)
            goto bail;
    }

bail:
    return dierr;
}

/*
 * Open the volume.  If it fails, open a placeholder instead.  (If *that*
 * fails, return with an error.)
 */
DIError DiskFSMicroDrive::OpenVol(int idx, long startBlock, long numBlocks)
{
    DIError dierr;

    dierr = OpenSubVolume(startBlock, numBlocks);
    if (dierr != kDIErrNone) {
        if (dierr == kDIErrCancelled)
            goto bail;
        DiskFS* pNewFS = NULL;
        DiskImg* pNewImg = NULL;

        LOGW(" MicroDrive failed opening sub-volume %d", idx);
        dierr = CreatePlaceholder(startBlock, numBlocks, NULL, NULL,
                    &pNewImg, &pNewFS);
        if (dierr == kDIErrNone) {
            AddSubVolumeToList(pNewImg, pNewFS);
        } else {
            LOGE("  MicroDrive unable to create placeholder (err=%d)",
                dierr);
            // fall out with error
        }
    }

bail:
    return dierr;
}
