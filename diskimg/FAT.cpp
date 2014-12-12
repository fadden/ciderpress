/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Implementation of the Windows FAT filesystem.
 *
 * Right now we just try to identify that a disk is in a PC format rather
 * than Apple II.  The trick here is to figure out whether block 0 is a
 * Master Boot Record or merely a Boot Sector.
 */
#include "StdAfx.h"
#include "DiskImgPriv.h"


/*
 * ===========================================================================
 *      DiskFSFAT
 * ===========================================================================
 */

const int kBlkSize = 512;
const long kBootBlock = 0;
const uint16_t kSignature = 0xaa55;   // MBR or boot sector
const int kSignatureOffset = 0x1fe;
const uint8_t kOpcodeMumble = 0x33;   // seen on 2nd drive
const uint8_t kOpcodeBranch = 0xeb;
const uint8_t kOpcodeSetInt = 0xfa;

typedef struct PartitionTableEntry {
    uint8_t     driveNum;           // dl (0x80 or 0x00)
    uint8_t     startHead;          // dh
    uint8_t     startSector;        // cl (&0x3f=sector, +two hi bits cyl)
    uint8_t     startCylinder;      // ch (low 8 bits of 10-bit cylinder)
    uint8_t     type;               // partition type
    uint8_t     endHead;            // dh
    uint8_t     endSector;          // cl
    uint8_t     endCylinder;        // ch
    uint32_t    startLBA;           // in blocks
    uint32_t    size;               // in blocks
} PartitionTableEntry;

/*
 * Definition of a Master Boot Record, which is block 0 of a physical volume.
 */
typedef struct DiskFSFAT::MasterBootRecord {
    /*
     * Begins immediately with code, usually 0xfa (set interrupt flag) or
     * 0xeb (relative branch).
     */
    uint8_t     firstByte;

    /*
     * Partition table starts at 0x1be.  Four entries, each 16 bytes.
     */
    PartitionTableEntry parTab[4];
} MasterBootRecord;

/*
 * Definition of a boot sector, which is block 0 of a logical volume.
 */
typedef struct DiskFSFAT::BootSector {
    /*
     * The first few bytes of the boot sector is called the BIOS Parameter
     * Block, or BPB.
     */
    uint8_t     jump[3];            // usually EB XX 90
    uint8_t     oemName[8];         // e.g. "MSWIN4.1" or "MSDOS5.0"
    uint16_t    bytesPerSector;     // usually (always?) 512
    uint8_t     sectPerCluster;
    uint16_t    reservedSectors;
    uint8_t     numFAT;
    uint16_t    numRootDirEntries;
    uint16_t    numSectors;         // if set, ignore numSectorsHuge
    uint8_t     mediaType;
    uint16_t    numFATSectors;
    uint16_t    sectorsPerTrack;
    uint16_t    numHeads;
    uint32_t    numHiddenSectors;
    uint32_t    numSectorsHuge;     // only if numSectors==0
    /*
     * This next part can start immediately after the above (at 0x24) for
     * FAT12/FAT16, or somewhat later (0x42) for FAT32.  It doesn't seem
     * to exist for NTFS.  Probably safest to assume it doesn't exist.
     *
     * The only way to be sure of what we're dealing with is to know the
     * partition type, but if this is our block 0 then we can't know what
     * that is.
     */
    uint8_t     driveNum;
    uint8_t     reserved;
    uint8_t     signature;          // 0x29
    uint32_t    volumeID;
    uint8_t     volumeLabel[11];    // e.g. "FUBAR      "
    uint8_t     fileSysType[8];     // e.g. "FAT12   "

    /*
     * Code follows.  Signature 0xaa55 in the last two bytes.
     */
} BootSector;

// some values for MediaType
enum MediaType {
    kMediaTypeLarge = 0xf0,         // 1440KB or 2800KB 3.5" disk
    kMediaTypeHardDrive = 0xf8,
    kMediaTypeMedium = 0xf9,        // 720KB 3.5" disk or 1.2MB 5.25" disk
    kMediaTypeSmall = 0xfd,         // 360KB 5.25" disk
};


/*
 * Unpack the MBR.
 *
 * Returns "true" if this looks like an MBR, "false" otherwise.
 */
/*static*/ bool DiskFSFAT::UnpackMBR(const uint8_t* buf, MasterBootRecord* pOut)
{
    const uint8_t* ptr;
    int i;

    pOut->firstByte = buf[0x00];

    ptr = &buf[0x1be];
    for (i = 0; i < 4; i++) {
        pOut->parTab[i].driveNum = ptr[0x00];
        pOut->parTab[i].startHead = ptr[0x01];
        pOut->parTab[i].startSector = ptr[0x02];
        pOut->parTab[i].startCylinder = ptr[0x03];
        pOut->parTab[i].type = ptr[0x04];
        pOut->parTab[i].endHead = ptr[0x05];
        pOut->parTab[i].endSector = ptr[0x06];
        pOut->parTab[i].endCylinder = ptr[0x07];
        pOut->parTab[i].startLBA = GetLongLE(&ptr[0x08]);
        pOut->parTab[i].size = GetLongLE(&ptr[0x0c]);

        ptr += 16;
    }

    if (pOut->firstByte != kOpcodeBranch &&
        pOut->firstByte != kOpcodeSetInt &&
        pOut->firstByte != kOpcodeMumble)
        return false;
    bool foundActive = false;
    for (i = 0; i < 4; i++) {
        if (pOut->parTab[i].driveNum == 0x80)
            foundActive = true;
        else if (pOut->parTab[i].driveNum != 0x00)
            return false;   // must be 0x00 or 0x80
    }
    // CFFA cards don't seem to set the "active" flag
    if (false && !foundActive)
        return false;
    return true;
}

/*
 * Unpack the boot sector.
 *
 * Returns "true" if this looks like a boot sector, "false" otherwise.
 */
/*static*/ bool DiskFSFAT::UnpackBootSector(const uint8_t* buf, BootSector* pOut)
{
    memcpy(pOut->jump, &buf[0x00], sizeof(pOut->jump));
    memcpy(pOut->oemName, &buf[0x03], sizeof(pOut->oemName));
    pOut->bytesPerSector = GetShortLE(&buf[0x0b]);
    pOut->sectPerCluster = buf[0x0d];
    pOut->reservedSectors = GetShortLE(&buf[0x0e]);
    pOut->numFAT = buf[0x10];
    pOut->numRootDirEntries = GetShortLE(&buf[0x11]);
    pOut->numSectors = GetShortLE(&buf[0x13]);
    pOut->mediaType = buf[0x15];
    pOut->numFATSectors = GetShortLE(&buf[0x16]);
    pOut->sectorsPerTrack = GetShortLE(&buf[0x18]);
    pOut->numHeads = GetShortLE(&buf[0x1a]);
    pOut->numHiddenSectors = GetLongLE(&buf[0x1c]);
    pOut->numSectorsHuge = GetLongLE(&buf[0x20]);

    if (pOut->jump[0] != kOpcodeBranch && pOut->jump[0] != kOpcodeSetInt)
        return false;
    if (pOut->bytesPerSector != 512)
        return false;
    return true;
}

/*
 * See if this looks like a FAT volume.
 */
/*static*/ DIError DiskFSFAT::TestImage(DiskImg* pImg, DiskImg::SectorOrder imageOrder)
{
    DIError dierr = kDIErrNone;
    uint8_t blkBuf[kBlkSize];
    MasterBootRecord mbr;
    BootSector bs;

    dierr = pImg->ReadBlockSwapped(kBootBlock, blkBuf, imageOrder,
                DiskImg::kSectorOrderProDOS);
    if (dierr != kDIErrNone)
        goto bail;

    /*
     * Both MBR and boot sectors have the same signature in block 0.
     */
    if (GetShortLE(&blkBuf[kSignatureOffset]) != kSignature) {
        dierr = kDIErrFilesystemNotFound;
        goto bail;
    }

    /*
     * Decode it as an MBR and as a partition table.  Figure out which
     * one makes sense.  If neither make sense, fail.
     */
    bool hasMBR, hasBS;
    hasMBR = UnpackMBR(blkBuf, &mbr);
    hasBS = UnpackBootSector(blkBuf, &bs);
    LOGI("  FAT hasMBR=%d hasBS=%d", hasMBR, hasBS);

    if (!hasMBR && !hasBS) {
        dierr = kDIErrFilesystemNotFound;
        goto bail;
    }
    if (hasMBR) {
        LOGI(" FAT partition table found:");
        for (int i = 0; i < 4; i++) {
            LOGI("   %d: type=0x%02x start LBA=%-9u size=%u",
                i, mbr.parTab[i].type,
                mbr.parTab[i].startLBA, mbr.parTab[i].size);
        }
    }
    if (hasBS) {
        LOGI(" FAT boot sector found:");
        LOGI("   OEMName is '%.8s'", bs.oemName);
    }

    // looks good!

bail:
    return dierr;
}

/*
 * Test to see if the image is a FAT disk.
 */
/*static*/ DIError DiskFSFAT::TestFS(DiskImg* pImg, DiskImg::SectorOrder* pOrder,
    DiskImg::FSFormat* pFormat, FSLeniency leniency)
{
    /* must be block format, should be at least 360K */
    if (!pImg->GetHasBlocks() || pImg->GetNumBlocks() < kExpectedMinBlocks)
        return kDIErrFilesystemNotFound;
    if (pImg->GetIsEmbedded())      // don't look for FAT inside CFFA!
        return kDIErrFilesystemNotFound;

    DiskImg::SectorOrder ordering[DiskImg::kSectorOrderMax];
    
    DiskImg::GetSectorOrderArray(ordering, *pOrder);

    for (int i = 0; i < DiskImg::kSectorOrderMax; i++) {
        if (ordering[i] == DiskImg::kSectorOrderUnknown)
            continue;
        if (TestImage(pImg, ordering[i]) == kDIErrNone) {
            *pOrder = ordering[i];
            *pFormat = DiskImg::kFormatMSDOS;
            return kDIErrNone;
        }
    }

    LOGI(" FAT didn't find valid FS");
    return kDIErrFilesystemNotFound;
}

/*
 * Get things rolling.
 */
DIError DiskFSFAT::Initialize(void)
{
    DIError dierr = kDIErrNone;

    strcpy(fVolumeName, "[MS-DOS]");    // max 11 chars
    strcpy(fVolumeID, "FATxx [MS-DOS]");

    // take the easy way out
    fTotalBlocks = fpImg->GetNumBlocks();

    CreateFakeFile();

    SetVolumeUsageMap();

    return dierr;
}


/*
 * Blank out the volume usage map.
 */
void DiskFSFAT::SetVolumeUsageMap(void)
{
    VolumeUsage::ChunkState cstate;
    long block;

    fVolumeUsage.Create(fpImg->GetNumBlocks());

    cstate.isUsed = true;
    cstate.isMarkedUsed = true;
    cstate.purpose = VolumeUsage::kChunkPurposeUnknown;

    for (block = fTotalBlocks-1; block >= 0; block--)
        fVolumeUsage.SetChunkState(block, &cstate);
}


/*
 * Fill a buffer with some interesting stuff, and add it to the file list.
 */
void DiskFSFAT::CreateFakeFile(void)
{
    A2FileFAT* pFile;
    char buf[768];      // currently running about 430
    static const char* kFormatMsg =
"The FAT12/16/32 and NTFS filesystems are not supported.  CiderPress knows\r\n"
"how to recognize MS-DOS and Windows volumes so that it can identify\r\n"
"PC data on removable media, but it does not know how to view or extract\r\n"
"files from them.\r\n"
"\r\n"
"Some information about this FAT volume:\r\n"
"\r\n"
"  Volume name : '%s'\r\n"
"  Volume size : %ld blocks (%.2fMB)\r\n"
"\r\n"
"(CiderPress limits itself to 8GB, so larger volume sizes may not be shown.)\r\n"
;
    long capacity;

    capacity = fTotalBlocks;

    memset(buf, 0, sizeof(buf));
    snprintf(buf, NELEM(buf)-1, kFormatMsg,
        fVolumeName,
        capacity,
        (double) capacity / 2048.0);

    pFile = new A2FileFAT(this);
    pFile->SetFakeFile(buf, strlen(buf));
    strcpy(pFile->fFileName, "(not supported)");

    AddFileToList(pFile);
}


/*
 * ===========================================================================
 *      A2FileFAT
 * ===========================================================================
 */

/*
 * Dump the contents of the A2File structure.
 */
void A2FileFAT::Dump(void) const
{
    LOGD("A2FileFAT '%s'", fFileName);
}

/*
 * Not a whole lot to do.
 */
DIError A2FileFAT::Open(A2FileDescr** ppOpenFile, bool readOnly,
    bool rsrcFork /*=false*/)
{
    A2FDFAT* pOpenFile = NULL;

    if (fpOpenFile != NULL)
        return kDIErrAlreadyOpen;
    if (rsrcFork)
        return kDIErrForkNotFound;
    assert(readOnly == true);

    pOpenFile = new A2FDFAT(this);

    fpOpenFile = pOpenFile;
    *ppOpenFile = pOpenFile;

    return kDIErrNone;
}


/*
 * ===========================================================================
 *      A2FDFAT
 * ===========================================================================
 */

/*
 * Read a chunk of data from the fake file.
 */
DIError A2FDFAT::Read(void* buf, size_t len, size_t* pActual)
{
    LOGD(" FAT reading %lu bytes from '%s' (offset=%ld)",
        (unsigned long) len, fpFile->GetPathName(), (long) fOffset);

    A2FileFAT* pFile = (A2FileFAT*) fpFile;

    /* don't allow them to read past the end of the file */
    if (fOffset + (long)len > pFile->fLength) {
        if (pActual == NULL)
            return kDIErrDataUnderrun;
        len = (size_t) (pFile->fLength - fOffset);
    }
    if (pActual != NULL)
        *pActual = len;

    memcpy(buf, pFile->GetFakeFileBuf(), len);

    fOffset += len;

    return kDIErrNone;
}

/*
 * Write data at the current offset.
 */
DIError A2FDFAT::Write(const void* buf, size_t len, size_t* pActual)
{
    return kDIErrNotSupported;
}

/*
 * Seek to a new offset.
 */
DIError A2FDFAT::Seek(di_off_t offset, DIWhence whence)
{
    di_off_t fileLen = ((A2FileFAT*) fpFile)->fLength;

    switch (whence) {
    case kSeekSet:
        if (offset < 0 || offset > fileLen)
            return kDIErrInvalidArg;
        fOffset = offset;
        break;
    case kSeekEnd:
        if (offset > 0 || offset < -fileLen)
            return kDIErrInvalidArg;
        fOffset = fileLen + offset;
        break;
    case kSeekCur:
        if (offset < -fOffset ||
            offset >= (fileLen - fOffset))
        {
            return kDIErrInvalidArg;
        }
        fOffset += offset;
        break;
    default:
        assert(false);
        return kDIErrInvalidArg;
    }

    assert(fOffset >= 0 && fOffset <= fileLen);
    return kDIErrNone;
}

/*
 * Return current offset.
 */
di_off_t A2FDFAT::Tell(void)
{
    return fOffset;
}

/*
 * Release file state, and tell our parent to destroy us.
 */
DIError A2FDFAT::Close(void)
{
    fpFile->CloseDescr(this);
    return kDIErrNone;
}

/*
 * Return the #of sectors/blocks in the file.
 */
long A2FDFAT::GetSectorCount(void) const
{
    A2FileFAT* pFile = (A2FileFAT*) fpFile;
    return (long) ((pFile->fLength+255) / 256);
}

long A2FDFAT::GetBlockCount(void) const
{
    A2FileFAT* pFile = (A2FileFAT*) fpFile;
    return (long) ((pFile->fLength+511) / 512);
}

/*
 * Return the Nth track/sector in this file.
 */
DIError A2FDFAT::GetStorage(long sectorIdx, long* pTrack, long* pSector) const
{
    return kDIErrNotSupported;
}

/*
 * Return the Nth 512-byte block in this file.
 */
DIError A2FDFAT::GetStorage(long blockIdx, long* pBlock) const
{
    return kDIErrNotSupported;
}
