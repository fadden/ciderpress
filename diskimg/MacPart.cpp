/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * The "MacPart" DiskFS is a container class for multiple ProDOS and HFS
 * volumes.  It represents a partitioned disk device, such as a hard
 * drive or CD-ROM.
 */
#include "StdAfx.h"
#include "DiskImgPriv.h"


const int kBlkSize = 512;
const int kDDRBlock = 0;        // Driver Descriptor Record block
const int kPartMapStart = 1;    // start of partition map


/*
 * Format of DDR (block 0).
 */
typedef struct DiskFSMacPart::DriverDescriptorRecord {
    uint16_t    sbSig;              // {device signature}
    uint16_t    sbBlkSize;          // {block size of the device}
    uint32_t    sbBlkCount;         // {number of blocks on the device}
    uint16_t    sbDevType;          // {reserved}
    uint16_t    sbDevId;            // {reserved}
    uint32_t    sbData;             // {reserved}
    uint16_t    sbDrvrCount;        // {number of driver descriptor entries}
    uint16_t    hiddenPad;          // implicit in specification
    uint32_t    ddBlock;            // {first driver's starting block}
    uint16_t    ddSize;             // {size of the driver, in 512-byte blocks}
    uint16_t    ddType;             // {operating system type (MacOS = 1)}
    uint16_t    ddPad[242];         // {additional drivers, if any}
} DriverDescriptorRecord;

/*
 * Format of partition map blocks.  The partition map is an array of these.
 */
typedef struct DiskFSMacPart::PartitionMap {
    uint16_t    pmSig;              // {partition signature}
    uint16_t    pmSigPad;           // {reserved}
    uint32_t    pmMapBlkCnt;        // {number of blocks in partition map}
    uint32_t    pmPyPartStart;      // {first physical block of partition}
    uint32_t    pmPartBlkCnt;       // {number of blocks in partition}
    uint8_t     pmPartName[32];     // {partition name}
    uint8_t     pmParType[32];      // {partition type}
    uint32_t    pmLgDataStart;      // {first logical block of data area}
    uint32_t    pmDataCnt;          // {number of blocks in data area}
    uint32_t    pmPartStatus;       // {partition status information}
    uint32_t    pmLgBootStart;      // {first logical block of boot code}
    uint32_t    pmBootSize;         // {size of boot code, in bytes}
    uint32_t    pmBootAddr;         // {boot code load address}
    uint32_t    pmBootAddr2;        // {reserved}
    uint32_t    pmBootEntry;        // {boot code entry point}
    uint32_t    pmBootEntry2;       // {reserved}
    uint32_t    pmBootCksum;        // {boot code checksum}
    uint8_t     pmProcessor[16];    // {processor type}
    uint16_t    pmPad[188];         // {reserved}
} PartitionMap;


/*
 * Figure out if this is a Macintosh-style partition.
 *
 * The "imageOrder" parameter has no use here, because (in the current
 * version) embedded parent volumes are implicitly ProDOS-ordered.
 *
 * It would be difficult to guess the block order based on the partition
 * structure, because the partition map entries can appear in any order.
 */
/*static*/ DIError DiskFSMacPart::TestImage(DiskImg* pImg,
    DiskImg::SectorOrder imageOrder)
{
    DIError dierr = kDIErrNone;
    uint8_t blkBuf[kBlkSize];
    DriverDescriptorRecord ddr;
    long pmMapBlkCnt;

    assert(sizeof(PartitionMap) == kBlkSize);
    assert(sizeof(DriverDescriptorRecord) == kBlkSize);

    /* check the DDR block */
    dierr = pImg->ReadBlockSwapped(kDDRBlock, blkBuf, imageOrder,
                DiskImg::kSectorOrderProDOS);
    if (dierr != kDIErrNone)
        goto bail;

    UnpackDDR(blkBuf, &ddr);

    if (ddr.sbSig != kDDRSignature) {
        dierr = kDIErrFilesystemNotFound;
        goto bail;
    }
    if (ddr.sbBlkSize != kBlkSize || ddr.sbBlkCount == 0) {
        if (ddr.sbBlkSize == 0 && ddr.sbBlkCount == 0) {
            /*
             * This is invalid, but it's the way floptical images formatted
             * by the C.V.Tech format utilities look.
             */
            LOGI(" MacPart NOTE: found zeroed-out DDR, continuing anyway");
        } else if (ddr.sbBlkSize == kBlkSize && ddr.sbBlkCount == 0) {
            /*
             * This showed up on a disc, so handle it too.
             */
            LOGI(" MacPart NOTE: found partially-zeroed-out DDR, continuing");
        } else {
            LOGI(" MacPart found 'ER' signature but blkSize=%d blkCount=%d",
                ddr.sbBlkSize, ddr.sbBlkCount);
            dierr = kDIErrFilesystemNotFound;
            goto bail;
        }
    }
    DumpDDR(&ddr);

    /* make sure block 1 is a partition */
    dierr = pImg->ReadBlockSwapped(kPartMapStart, blkBuf, imageOrder,
                DiskImg::kSectorOrderProDOS);
    if (dierr != kDIErrNone)
        goto bail;

    if (GetShortBE(&blkBuf[0x00]) != kPartitionSignature) {
        LOGI(" MacPart partition signature not found in first part block");
        dierr = kDIErrFilesystemNotFound;
        goto bail;
    }
    pmMapBlkCnt = GetLongBE(&blkBuf[0x04]);
    if (pmMapBlkCnt <= 0 || pmMapBlkCnt > 256) {
        LOGI(" MacPart unreasonable pmMapBlkCnt value %ld",
            pmMapBlkCnt);
        dierr = kDIErrFilesystemNotFound;
        goto bail;
    }

    /* could test the rest -- might fix "imageOrder", might not -- but
       the format is pretty unambiguous, and we don't care about the order */

    // success!
    LOGI(" MacPart partition map block count = %ld", pmMapBlkCnt);

bail:
    return dierr;
}

/*
 * Unpack a DDR disk block into a DDR data structure.
 */
/*static*/ void DiskFSMacPart::UnpackDDR(const uint8_t* buf,
    DriverDescriptorRecord* pDDR)
{
    pDDR->sbSig = GetShortBE(&buf[0x00]);
    pDDR->sbBlkSize = GetShortBE(&buf[0x02]);
    pDDR->sbBlkCount = GetLongBE(&buf[0x04]);
    pDDR->sbDevType = GetShortBE(&buf[0x08]);
    pDDR->sbDevId = GetShortBE(&buf[0x0a]);
    pDDR->sbData = GetLongBE(&buf[0x0c]);
    pDDR->sbDrvrCount = GetShortBE(&buf[0x10]);
    pDDR->hiddenPad = GetShortBE(&buf[0x12]);
    pDDR->ddBlock = GetLongBE(&buf[0x14]);
    pDDR->ddSize = GetShortBE(&buf[0x18]);
    pDDR->ddType = GetShortBE(&buf[0x1a]);

    int i;
    for (i = 0; i < (int) NELEM(pDDR->ddPad); i++) {
        pDDR->ddPad[i] = GetShortBE(&buf[0x1c] + i * sizeof(pDDR->ddPad[0]));
    }
    assert(0x1c + i * sizeof(pDDR->ddPad[0]) == (unsigned int) kBlkSize);
}

/*
 * Debug: dump the contents of the DDR.
 */
/*static*/ void DiskFSMacPart::DumpDDR(const DriverDescriptorRecord* pDDR)
{
    LOGI(" MacPart driver descriptor record");
    LOGI("    sbSig=0x%04x sbBlkSize=%d sbBlkCount=%d",
        pDDR->sbSig, pDDR->sbBlkSize, pDDR->sbBlkCount);
    LOGI("    sbDevType=%d sbDevId=%d sbData=%d sbDrvrCount=%d",
        pDDR->sbDevType, pDDR->sbDevId, pDDR->sbData, pDDR->sbDrvrCount);
    LOGI("    (pad=%d) ddBlock=%d ddSize=%d ddType=%d",
        pDDR->hiddenPad, pDDR->ddBlock, pDDR->ddSize, pDDR->ddType);
}

/*
 * Unpack a partition map disk block into a partition map data structure.
 */
/*static*/ void DiskFSMacPart::UnpackPartitionMap(const uint8_t* buf,
    PartitionMap* pMap)
{
    pMap->pmSig = GetShortBE(&buf[0x00]);
    pMap->pmSigPad = GetShortBE(&buf[0x02]);
    pMap->pmMapBlkCnt = GetLongBE(&buf[0x04]);
    pMap->pmPyPartStart = GetLongBE(&buf[0x08]);
    pMap->pmPartBlkCnt = GetLongBE(&buf[0x0c]);
    memcpy(pMap->pmPartName, &buf[0x10], sizeof(pMap->pmPartName));
    pMap->pmPartName[sizeof(pMap->pmPartName)-1] = '\0';
    memcpy(pMap->pmParType, &buf[0x30], sizeof(pMap->pmParType));
    pMap->pmParType[sizeof(pMap->pmParType)-1] = '\0';
    pMap->pmLgDataStart = GetLongBE(&buf[0x50]);
    pMap->pmDataCnt = GetLongBE(&buf[0x54]);
    pMap->pmPartStatus = GetLongBE(&buf[0x58]);
    pMap->pmLgBootStart = GetLongBE(&buf[0x5c]);
    pMap->pmBootSize = GetLongBE(&buf[0x60]);
    pMap->pmBootAddr = GetLongBE(&buf[0x64]);
    pMap->pmBootAddr2 = GetLongBE(&buf[0x68]);
    pMap->pmBootEntry = GetLongBE(&buf[0x6c]);
    pMap->pmBootEntry2 = GetLongBE(&buf[0x70]);
    pMap->pmBootCksum = GetLongBE(&buf[0x74]);
    memcpy((char*) pMap->pmProcessor, &buf[0x78], sizeof(pMap->pmProcessor));
    pMap->pmProcessor[sizeof(pMap->pmProcessor)-1] = '\0';

    int i;
    for (i = 0; i < (int) NELEM(pMap->pmPad); i++) {
        pMap->pmPad[i] = GetShortBE(&buf[0x88] + i * sizeof(pMap->pmPad[0]));
    }
    assert(0x88 + i * sizeof(pMap->pmPad[0]) == (unsigned int) kBlkSize);
}

/*
 * Debug: dump the contents of the partition map.
 */
/*static*/ void DiskFSMacPart::DumpPartitionMap(long block, const PartitionMap* pMap)
{
    LOGI(" MacPart partition map: block=%ld", block);
    LOGI("    pmSig=0x%04x (pad=0x%04x)  pmMapBlkCnt=%d",
        pMap->pmSig, pMap->pmSigPad, pMap->pmMapBlkCnt);
    LOGI("    pmPartName='%s' pmParType='%s'",
        pMap->pmPartName, pMap->pmParType);
    LOGI("    pmPyPartStart=%d pmPartBlkCnt=%d",
        pMap->pmPyPartStart, pMap->pmPartBlkCnt);
    LOGI("    pmLgDataStart=%d pmDataCnt=%d",
        pMap->pmLgDataStart, pMap->pmDataCnt);
    LOGI("    pmPartStatus=%d",
        pMap->pmPartStatus);
    LOGI("    pmLgBootStart=%d pmBootSize=%d",
        pMap->pmLgBootStart, pMap->pmBootSize);
    LOGI("    pmBootAddr=%d pmBootAddr2=%d pmBootEntry=%d pmBootEntry2=%d",
        pMap->pmBootAddr, pMap->pmBootAddr2,
        pMap->pmBootEntry, pMap->pmBootEntry2);
    LOGI("    pmBootCksum=%d pmProcessor='%s'",
        pMap->pmBootCksum, pMap->pmProcessor);
}


/*
 * Open up a sub-volume.
 */
DIError DiskFSMacPart::OpenSubVolume(const PartitionMap* pMap)
{
    DIError dierr = kDIErrNone;
    DiskFS* pNewFS = NULL;
    DiskImg* pNewImg = NULL;
    long startBlock, numBlocks;
    bool tweaked = false;

    assert(pMap != NULL);
    startBlock = pMap->pmPyPartStart;
    numBlocks = pMap->pmPartBlkCnt;

    LOGI("Adding '%s' (%s) %ld +%ld",
        pMap->pmPartName, pMap->pmParType, startBlock, numBlocks);

    if (startBlock > fpImg->GetNumBlocks()) {
        LOGI("MacPart start block out of range (%ld vs %ld)",
            startBlock, fpImg->GetNumBlocks());
        return kDIErrBadPartition;
    }
    if (startBlock + numBlocks > fpImg->GetNumBlocks()) {
        LOGI("MacPart partition too large (%ld vs %ld avail)",
            numBlocks, fpImg->GetNumBlocks() - startBlock);
        fpImg->AddNote(DiskImg::kNoteInfo,
            "Reduced partition '%s' (%s) from %ld blocks to %ld.\n",
            pMap->pmPartName, pMap->pmParType, numBlocks,
            fpImg->GetNumBlocks() - startBlock);
        numBlocks = fpImg->GetNumBlocks() - startBlock;
        tweaked = true;
    }

    pNewImg = new DiskImg;
    if (pNewImg == NULL) {
        dierr = kDIErrMalloc;
        goto bail;
    }

    /*
     * If "tweaked" is true, we want to make the volume read-only, so that the
     * volume copier doesn't stomp on it (on the off chance we've got it
     * wrong).  However, that won't stop the volume copier from stomping on
     * the entire thing, so we really need to change *all* members of the
     * diskimg tree to be read-only.  This seems counter-productive though.
     *
     * So far the only actual occurrence of tweakedness was from the first
     * Apple "develop" CD-ROM, which had a bad Apple_Extra partition on the
     * end.
     */
    (void) tweaked;

    dierr = pNewImg->OpenImage(fpImg, startBlock, numBlocks);
    if (dierr != kDIErrNone) {
        LOGI(" MacPartSub: OpenImage(%ld,%ld) failed (err=%d)",
            startBlock, numBlocks, dierr);
        goto bail;
    }

    //LOGI("  +++ CFFASub: new image has ro=%d (parent=%d)",
    //  pNewImg->GetReadOnly(), pImg->GetReadOnly());

    /* the partition is typed; currently no way to give hints to analyzer */
    dierr = pNewImg->AnalyzeImage();
    if (dierr != kDIErrNone) {
        LOGI(" MacPartSub: analysis failed (err=%d)", dierr);
        goto bail;
    }

    /* we allow unrecognized partitions */
    if (pNewImg->GetFSFormat() == DiskImg::kFormatUnknown ||
        pNewImg->GetSectorOrder() == DiskImg::kSectorOrderUnknown)
    {
        LOGI(" MacPartSub (%ld,%ld): unable to identify filesystem",
            startBlock, numBlocks);
        DiskFSUnknown* pUnknownFS = new DiskFSUnknown;
        if (pUnknownFS == NULL) {
            dierr = kDIErrInternal;
            goto bail;
        }
        pUnknownFS->SetVolumeInfo((const char*)pMap->pmParType);
        pNewFS = pUnknownFS;
        pNewImg->AddNote(DiskImg::kNoteInfo, "Partition name='%s' type='%s'.",
            pMap->pmPartName, pMap->pmParType);
    } else {
        /* open a DiskFS for the sub-image */
        LOGI(" MacPartSub (%ld,%ld) analyze succeeded!", startBlock, numBlocks);
        pNewFS = pNewImg->OpenAppropriateDiskFS(true);
        if (pNewFS == NULL) {
            LOGI(" MacPartSub: OpenAppropriateDiskFS failed");
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
        LOGI(" MacPartSub: error %d reading list of files from disk", dierr);
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
 * Check to see if this is a MacPart volume.
 */
/*static*/ DIError DiskFSMacPart::TestFS(DiskImg* pImg, DiskImg::SectorOrder* pOrder,
    DiskImg::FSFormat* pFormat, FSLeniency leniency)
{
    if (pImg->GetNumBlocks() < kMinInterestingBlocks)
        return kDIErrFilesystemNotFound;
    if (pImg->GetIsEmbedded())      // don't look for partitions inside
        return kDIErrFilesystemNotFound;

    /* assume ProDOS -- shouldn't matter, since it's embedded */
    if (TestImage(pImg, DiskImg::kSectorOrderProDOS) == kDIErrNone) {
        *pFormat = DiskImg::kFormatMacPart;
        *pOrder = DiskImg::kSectorOrderProDOS;
        return kDIErrNone;
    }

    LOGI("  FS didn't find valid MacPart");
    return kDIErrFilesystemNotFound;
}

/*
 * Prep the MacPart "container" for use.
 */
DIError DiskFSMacPart::Initialize(void)
{
    DIError dierr = kDIErrNone;

    LOGI("MacPart initializing (scanForSub=%d)", fScanForSubVolumes);

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
 * Because the partitions are explicitly typed, we don't need to probe
 * their contents.  But we do anyway.
 */
DIError DiskFSMacPart::FindSubVolumes(void)
{
    DIError dierr = kDIErrNone;
    uint8_t buf[kBlkSize];
    PartitionMap map;
    int i, numMapBlocks;

    dierr = fpImg->ReadBlock(kPartMapStart, buf);
    if (dierr != kDIErrNone)
        goto bail;
    UnpackPartitionMap(buf, &map);
    numMapBlocks = map.pmMapBlkCnt;

    for (i = 0; i < numMapBlocks; i++) {
        if (i != 0) {
            dierr = fpImg->ReadBlock(kPartMapStart+i, buf);
            if (dierr != kDIErrNone)
                goto bail;
            UnpackPartitionMap(buf, &map);
        }
        DumpPartitionMap(kPartMapStart+i, &map);

        dierr = OpenSubVolume(&map);
        if (dierr != kDIErrNone) {
            if (dierr == kDIErrCancelled)
                goto bail;
            DiskFS* pNewFS = NULL;
            DiskImg* pNewImg = NULL;
            LOGI(" MacPart failed opening sub-volume %d", i);
            dierr = CreatePlaceholder(map.pmPyPartStart, map.pmPartBlkCnt,
                (const char*)map.pmPartName, (const char*)map.pmParType,
                &pNewImg, &pNewFS);
            if (dierr == kDIErrNone) {
                AddSubVolumeToList(pNewImg, pNewFS);
            } else {
                LOGI("  MacPart unable to create placeholder (err=%d)",
                    dierr);
                break;  // something's wrong -- bail out with error
            }
        }
    }

bail:
    return dierr;
}
