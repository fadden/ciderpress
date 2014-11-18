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
    unsigned short  sbSig;              // {device signature}
    unsigned short  sbBlkSize;          // {block size of the device}
    unsigned long   sbBlkCount;         // {number of blocks on the device}
    unsigned short  sbDevType;          // {reserved}
    unsigned short  sbDevId;            // {reserved}
    unsigned long   sbData;             // {reserved}
    unsigned short  sbDrvrCount;        // {number of driver descriptor entries}
    unsigned short  hiddenPad;          // implicit in specification
    unsigned long   ddBlock;            // {first driver's starting block}
    unsigned short  ddSize;             // {size of the driver, in 512-byte blocks}
    unsigned short  ddType;             // {operating system type (MacOS = 1)}
    unsigned short  ddPad[242];         // {additional drivers, if any}
} DriverDescriptorRecord;

/*
 * Format of partition map blocks.  The partition map is an array of these.
 */
typedef struct DiskFSMacPart::PartitionMap {
    unsigned short  pmSig;              // {partition signature}
    unsigned short  pmSigPad;           // {reserved}
    unsigned long   pmMapBlkCnt;        // {number of blocks in partition map}
    unsigned long   pmPyPartStart;      // {first physical block of partition}
    unsigned long   pmPartBlkCnt;       // {number of blocks in partition}
    unsigned char   pmPartName[32];     // {partition name}
    unsigned char   pmParType[32];      // {partition type}
    unsigned long   pmLgDataStart;      // {first logical block of data area}
    unsigned long   pmDataCnt;          // {number of blocks in data area}
    unsigned long   pmPartStatus;       // {partition status information}
    unsigned long   pmLgBootStart;      // {first logical block of boot code}
    unsigned long   pmBootSize;         // {size of boot code, in bytes}
    unsigned long   pmBootAddr;         // {boot code load address}
    unsigned long   pmBootAddr2;        // {reserved}
    unsigned long   pmBootEntry;        // {boot code entry point}
    unsigned long   pmBootEntry2;       // {reserved}
    unsigned long   pmBootCksum;        // {boot code checksum}
    unsigned char   pmProcessor[16];    // {processor type}
    unsigned short  pmPad[188];         // {reserved}
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
/*static*/ DIError
DiskFSMacPart::TestImage(DiskImg* pImg, DiskImg::SectorOrder imageOrder)
{
    DIError dierr = kDIErrNone;
    unsigned char blkBuf[kBlkSize];
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
            WMSG0(" MacPart NOTE: found zeroed-out DDR, continuing anyway\n");
        } else if (ddr.sbBlkSize == kBlkSize && ddr.sbBlkCount == 0) {
            /*
             * This showed up on a disc, so handle it too.
             */
            WMSG0(" MacPart NOTE: found partially-zeroed-out DDR, continuing\n");
        } else {
            WMSG2(" MacPart found 'ER' signature but blkSize=%d blkCount=%ld\n",
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
        WMSG0(" MacPart partition signature not found in first part block\n");
        dierr = kDIErrFilesystemNotFound;
        goto bail;
    }
    pmMapBlkCnt = GetLongBE(&blkBuf[0x04]);
    if (pmMapBlkCnt <= 0 || pmMapBlkCnt > 256) {
        WMSG1(" MacPart unreasonable pmMapBlkCnt value %ld\n",
            pmMapBlkCnt);
        dierr = kDIErrFilesystemNotFound;
        goto bail;
    }

    /* could test the rest -- might fix "imageOrder", might not -- but
       the format is pretty unambiguous, and we don't care about the order */

    // success!
    WMSG1(" MacPart partition map block count = %ld\n", pmMapBlkCnt);

bail:
    return dierr;
}

/*
 * Unpack a DDR disk block into a DDR data structure.
 */
/*static*/ void
DiskFSMacPart::UnpackDDR(const unsigned char* buf,
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
/*static*/ void
DiskFSMacPart::DumpDDR(const DriverDescriptorRecord* pDDR)
{
    WMSG0(" MacPart driver descriptor record\n");
    WMSG3("    sbSig=0x%04x sbBlkSize=%d sbBlkCount=%ld\n",
        pDDR->sbSig, pDDR->sbBlkSize, pDDR->sbBlkCount);
    WMSG4("    sbDevType=%d sbDevId=%d sbData=%ld sbDrvrCount=%d\n",
        pDDR->sbDevType, pDDR->sbDevId, pDDR->sbData, pDDR->sbDrvrCount);
    WMSG4("    (pad=%d) ddBlock=%ld ddSize=%d ddType=%d\n",
        pDDR->hiddenPad, pDDR->ddBlock, pDDR->ddSize, pDDR->ddType);
}

/*
 * Unpack a partition map disk block into a partition map data structure.
 */
/*static*/ void
DiskFSMacPart::UnpackPartitionMap(const unsigned char* buf,
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
/*static*/ void
DiskFSMacPart::DumpPartitionMap(long block, const PartitionMap* pMap)
{
    WMSG1(" MacPart partition map: block=%ld\n", block);
    WMSG3("    pmSig=0x%04x (pad=0x%04x)  pmMapBlkCnt=%ld\n",
        pMap->pmSig, pMap->pmSigPad, pMap->pmMapBlkCnt);
    WMSG2("    pmPartName='%s' pmParType='%s'\n",
        pMap->pmPartName, pMap->pmParType);
    WMSG2("    pmPyPartStart=%ld pmPartBlkCnt=%ld\n",
        pMap->pmPyPartStart, pMap->pmPartBlkCnt);
    WMSG2("    pmLgDataStart=%ld pmDataCnt=%ld\n",
        pMap->pmLgDataStart, pMap->pmDataCnt);
    WMSG1("    pmPartStatus=%ld\n",
        pMap->pmPartStatus);
    WMSG2("    pmLgBootStart=%ld pmBootSize=%ld\n",
        pMap->pmLgBootStart, pMap->pmBootSize);
    WMSG4("    pmBootAddr=%ld pmBootAddr2=%ld pmBootEntry=%ld pmBootEntry2=%ld\n",
        pMap->pmBootAddr, pMap->pmBootAddr2,
        pMap->pmBootEntry, pMap->pmBootEntry2);
    WMSG2("    pmBootCksum=%ld pmProcessor='%s'\n",
        pMap->pmBootCksum, pMap->pmProcessor);
}


/*
 * Open up a sub-volume.
 */
DIError
DiskFSMacPart::OpenSubVolume(const PartitionMap* pMap)
{
    DIError dierr = kDIErrNone;
    DiskFS* pNewFS = NULL;
    DiskImg* pNewImg = NULL;
    long startBlock, numBlocks;
    bool tweaked = false;

    assert(pMap != NULL);
    startBlock = pMap->pmPyPartStart;
    numBlocks = pMap->pmPartBlkCnt;

    WMSG4("Adding '%s' (%s) %ld +%ld\n",
        pMap->pmPartName, pMap->pmParType, startBlock, numBlocks);

    if (startBlock > fpImg->GetNumBlocks()) {
        WMSG2("MacPart start block out of range (%ld vs %ld)\n",
            startBlock, fpImg->GetNumBlocks());
        return kDIErrBadPartition;
    }
    if (startBlock + numBlocks > fpImg->GetNumBlocks()) {
        WMSG2("MacPart partition too large (%ld vs %ld avail)\n",
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

    dierr = pNewImg->OpenImage(fpImg, startBlock, numBlocks);
    if (dierr != kDIErrNone) {
        WMSG3(" MacPartSub: OpenImage(%ld,%ld) failed (err=%d)\n",
            startBlock, numBlocks, dierr);
        goto bail;
    }

    //WMSG2("  +++ CFFASub: new image has ro=%d (parent=%d)\n",
    //  pNewImg->GetReadOnly(), pImg->GetReadOnly());

    /* the partition is typed; currently no way to give hints to analyzer */
    dierr = pNewImg->AnalyzeImage();
    if (dierr != kDIErrNone) {
        WMSG1(" MacPartSub: analysis failed (err=%d)\n", dierr);
        goto bail;
    }

    /* we allow unrecognized partitions */
    if (pNewImg->GetFSFormat() == DiskImg::kFormatUnknown ||
        pNewImg->GetSectorOrder() == DiskImg::kSectorOrderUnknown)
    {
        WMSG2(" MacPartSub (%ld,%ld): unable to identify filesystem\n",
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
        WMSG2(" MacPartSub (%ld,%ld) analyze succeeded!\n", startBlock, numBlocks);
        pNewFS = pNewImg->OpenAppropriateDiskFS(true);
        if (pNewFS == NULL) {
            WMSG0(" MacPartSub: OpenAppropriateDiskFS failed\n");
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
        WMSG1(" MacPartSub: error %d reading list of files from disk\n", dierr);
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
/*static*/ DIError
DiskFSMacPart::TestFS(DiskImg* pImg, DiskImg::SectorOrder* pOrder,
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

    WMSG0("  FS didn't find valid MacPart\n");
    return kDIErrFilesystemNotFound;
}


/*
 * Prep the MacPart "container" for use.
 */
DIError
DiskFSMacPart::Initialize(void)
{
    DIError dierr = kDIErrNone;

    WMSG1("MacPart initializing (scanForSub=%d)\n", fScanForSubVolumes);

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
DIError
DiskFSMacPart::FindSubVolumes(void)
{
    DIError dierr = kDIErrNone;
    unsigned char buf[kBlkSize];
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
            WMSG1(" MacPart failed opening sub-volume %d\n", i);
            dierr = CreatePlaceholder(map.pmPyPartStart, map.pmPartBlkCnt,
                (const char*)map.pmPartName, (const char*)map.pmParType,
                &pNewImg, &pNewFS);
            if (dierr == kDIErrNone) {
                AddSubVolumeToList(pNewImg, pNewFS);
            } else {
                WMSG1("  MacPart unable to create placeholder (err=%d)\n",
                    dierr);
                break;  // something's wrong -- bail out with error
            }
        }
    }

bail:
    return dierr;
}
