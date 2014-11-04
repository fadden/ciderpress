/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * This is a container for the Parsons Engineering FocusDrive.
 *
 * The format was reverse-engineered by Ranger Harke.
 */
#include "StdAfx.h"
#include "DiskImgPriv.h"


const int kBlkSize = 512;
const int kPartMapBlock = 0;    // partition map lives here
const int kMaxPartitions = 30;  // max allowed partitions on a drive
const int kPartNameStart = 1;   // partition names start here (2 blocks)
const int kPartNameLen = 32;    // max length of partition name

static const char* kSignature = "Parsons Engin.";
const int kSignatureLen = 14;

/*
 * Format of partition map.  It resides in the first 256 bytes of block 0.
 * All values are in little-endian order.
 *
 * We also make space here for the partition names, which live on blocks 1+2.
 */
typedef struct DiskFSFocusDrive::PartitionMap {
    unsigned char   signature[kSignatureLen];
    unsigned char   unknown1;
    unsigned char   partCount;      // could be ushort, combined w/unknown1
    unsigned char   unknown2[16];
    struct Entry {
        unsigned long   startBlock;
        unsigned long   blockCount;
        unsigned long   unknown1;
        unsigned long   unknown2;
        unsigned char   name[kPartNameLen+1];
    } entry[kMaxPartitions];
} PartitionMap;


/*
 * Figure out if this is a FocusDrive partition.
 *
 * The "imageOrder" parameter has no use here, because (in the current
 * version) embedded parent volumes are implicitly ProDOS-ordered.
 */
/*static*/ DIError
DiskFSFocusDrive::TestImage(DiskImg* pImg, DiskImg::SectorOrder imageOrder)
{
    DIError dierr = kDIErrNone;
    unsigned char blkBuf[kBlkSize];
    int partCount;

    /*
     * See if block 0 is a FocusDrive partition map.
     */
    dierr = pImg->ReadBlockSwapped(kPartMapBlock, blkBuf, imageOrder,
                DiskImg::kSectorOrderProDOS);
    if (dierr != kDIErrNone)
        goto bail;

    if (memcmp(blkBuf, kSignature, kSignatureLen) != 0) {
        WMSG0(" FocusDrive partition signature not found in first part block\n");
        dierr = kDIErrFilesystemNotFound;
        goto bail;
    }

    partCount = blkBuf[0x0f];
    if (partCount == 0 || partCount > kMaxPartitions) {
        WMSG1(" FocusDrive partition count looks bad (%d)\n", partCount);
        dierr = kDIErrFilesystemNotFound;
        goto bail;
    }

    // success!
    WMSG1(" Looks like FocusDrive with %d partitions\n", partCount);

bail:
    return dierr;
}


/*
 * Unpack a partition map block into a partition map data structure.
 */
/*static*/ void
DiskFSFocusDrive::UnpackPartitionMap(const unsigned char* buf,
    const unsigned char* nameBuf, PartitionMap* pMap)
{
    const unsigned char* ptr;
    const unsigned char* namePtr;
    int i;

    memcpy(pMap->signature, &buf[0x00], kSignatureLen);
    pMap->unknown1 = buf[0x0e];
    pMap->partCount = buf[0x0f];
    memcpy(pMap->unknown2, &buf[0x10], 16);

    ptr = &buf[0x20];
    namePtr = &nameBuf[kPartNameLen];   // not sure what first 32 bytes are
    for (i = 0; i < kMaxPartitions; i++) {
        pMap->entry[i].startBlock = GetLongLE(ptr);
        pMap->entry[i].blockCount = GetLongLE(ptr+4);
        pMap->entry[i].unknown1 = GetLongLE(ptr+8);
        pMap->entry[i].unknown2 = GetLongLE(ptr+12);

        memcpy(pMap->entry[i].name, namePtr, kPartNameLen);
        pMap->entry[i].name[kPartNameLen] = '\0';

        ptr += 0x10;
        namePtr += kPartNameLen;
    }

    assert(ptr == buf + kBlkSize);
}

/*
 * Debug: dump the contents of the partition map.
 */
/*static*/ void
DiskFSFocusDrive::DumpPartitionMap(const PartitionMap* pMap)
{
    int i;

    WMSG1(" FocusDrive partition map (%d partitions):\n", pMap->partCount);
    for (i = 0; i < pMap->partCount; i++) {
        WMSG4("  %2d: %8ld %8ld '%s'\n", i, pMap->entry[i].startBlock,
            pMap->entry[i].blockCount, pMap->entry[i].name);
    }
}


/*
 * Open up a sub-volume.
 */
DIError
DiskFSFocusDrive::OpenSubVolume(long startBlock, long numBlocks,
    const char* name)
{
    DIError dierr = kDIErrNone;
    DiskFS* pNewFS = nil;
    DiskImg* pNewImg = nil;
    //bool tweaked = false;

    WMSG2("Adding %ld +%ld\n", startBlock, numBlocks);

    if (startBlock > fpImg->GetNumBlocks()) {
        WMSG2("FocusDrive start block out of range (%ld vs %ld)\n",
            startBlock, fpImg->GetNumBlocks());
        return kDIErrBadPartition;
    }
    if (startBlock + numBlocks > fpImg->GetNumBlocks()) {
        WMSG2("FocusDrive partition too large (%ld vs %ld avail)\n",
            numBlocks, fpImg->GetNumBlocks() - startBlock);
        fpImg->AddNote(DiskImg::kNoteInfo,
            "Reduced partition from %ld blocks to %ld.\n",
            numBlocks, fpImg->GetNumBlocks() - startBlock);
        numBlocks = fpImg->GetNumBlocks() - startBlock;
        //tweaked = true;
    }

    pNewImg = new DiskImg;
    if (pNewImg == nil) {
        dierr = kDIErrMalloc;
        goto bail;
    }

    dierr = pNewImg->OpenImage(fpImg, startBlock, numBlocks);
    if (dierr != kDIErrNone) {
        WMSG3(" FocusDriveSub: OpenImage(%ld,%ld) failed (err=%d)\n",
            startBlock, numBlocks, dierr);
        goto bail;
    }

    //WMSG2("  +++ CFFASub: new image has ro=%d (parent=%d)\n",
    //  pNewImg->GetReadOnly(), pImg->GetReadOnly());

    /* figure out what the format is */
    dierr = pNewImg->AnalyzeImage();
    if (dierr != kDIErrNone) {
        WMSG1(" FocusDriveSub: analysis failed (err=%d)\n", dierr);
        goto bail;
    }

    /* we allow unrecognized partitions */
    if (pNewImg->GetFSFormat() == DiskImg::kFormatUnknown ||
        pNewImg->GetSectorOrder() == DiskImg::kSectorOrderUnknown)
    {
        WMSG2(" FocusDriveSub (%ld,%ld): unable to identify filesystem\n",
            startBlock, numBlocks);
        DiskFSUnknown* pUnknownFS = new DiskFSUnknown;
        if (pUnknownFS == nil) {
            dierr = kDIErrInternal;
            goto bail;
        }
        //pUnknownFS->SetVolumeInfo((const char*)pMap->pmParType);
        pNewFS = pUnknownFS;
    } else {
        /* open a DiskFS for the sub-image */
        WMSG2(" FocusDriveSub (%ld,%ld) analyze succeeded!\n", startBlock, numBlocks);
        pNewFS = pNewImg->OpenAppropriateDiskFS(true);
        if (pNewFS == nil) {
            WMSG0(" FocusDriveSub: OpenAppropriateDiskFS failed\n");
            dierr = kDIErrUnsupportedFSFmt;
            goto bail;
        }
    }
    pNewImg->AddNote(DiskImg::kNoteInfo, "Partition name='%s'.", name);

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
        WMSG1(" FocusDriveSub: error %d reading list of files from disk", dierr);
        goto bail;
    }

    /* add it to the list */
    AddSubVolumeToList(pNewImg, pNewFS);
    pNewImg = nil;
    pNewFS = nil;

bail:
    delete pNewFS;
    delete pNewImg;
    return dierr;
}

/*
 * Check to see if this is a FocusDrive volume.
 */
/*static*/ DIError
DiskFSFocusDrive::TestFS(DiskImg* pImg, DiskImg::SectorOrder* pOrder,
    DiskImg::FSFormat* pFormat, FSLeniency leniency)
{
    if (pImg->GetNumBlocks() < kMinInterestingBlocks)
        return kDIErrFilesystemNotFound;
    if (pImg->GetIsEmbedded())      // don't look for partitions inside
        return kDIErrFilesystemNotFound;

    /* assume ProDOS -- shouldn't matter, since it's embedded */
    if (TestImage(pImg, DiskImg::kSectorOrderProDOS) == kDIErrNone) {
        *pFormat = DiskImg::kFormatFocusDrive;
        *pOrder = DiskImg::kSectorOrderProDOS;
        return kDIErrNone;
    }

    WMSG0("  FS didn't find valid FocusDrive\n");
    return kDIErrFilesystemNotFound;
}


/*
 * Prep the FocusDrive "container" for use.
 */
DIError
DiskFSFocusDrive::Initialize(void)
{
    DIError dierr = kDIErrNone;

    WMSG1("FocusDrive initializing (scanForSub=%d)\n", fScanForSubVolumes);

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
DIError
DiskFSFocusDrive::FindSubVolumes(void)
{
    DIError dierr = kDIErrNone;
    unsigned char buf[kBlkSize];
    unsigned char nameBuf[kBlkSize*2];
    PartitionMap map;
    int i;

    dierr = fpImg->ReadBlock(kPartMapBlock, buf);
    if (dierr != kDIErrNone)
        goto bail;
    dierr = fpImg->ReadBlock(kPartNameStart, nameBuf);
    if (dierr != kDIErrNone)
        goto bail;
    dierr = fpImg->ReadBlock(kPartNameStart+1, nameBuf+kBlkSize);
    if (dierr != kDIErrNone)
        goto bail;
    UnpackPartitionMap(buf, nameBuf, &map);
    DumpPartitionMap(&map);

    for (i = 0; i < map.partCount; i++) {
        dierr = OpenVol(i, map.entry[i].startBlock, map.entry[i].blockCount,
                    (const char*)map.entry[i].name);
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
DIError
DiskFSFocusDrive::OpenVol(int idx, long startBlock, long numBlocks,
    const char* name)
{
    DIError dierr;

    dierr = OpenSubVolume(startBlock, numBlocks, name);
    if (dierr != kDIErrNone) {
        if (dierr == kDIErrCancelled)
            goto bail;
        DiskFS* pNewFS = nil;
        DiskImg* pNewImg = nil;

        WMSG1(" FocusDrive failed opening sub-volume %d\n", idx);
        dierr = CreatePlaceholder(startBlock, numBlocks, name, NULL,
                    &pNewImg, &pNewFS);
        if (dierr == kDIErrNone) {
            AddSubVolumeToList(pNewImg, pNewFS);
        } else {
            WMSG1("  FocusDrive unable to create placeholder (err=%d)\n",
                dierr);
            // fall out with error
        }
    }

bail:
    return dierr;
}
