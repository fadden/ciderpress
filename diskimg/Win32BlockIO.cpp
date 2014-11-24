/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Win32 block I/O routines.
 *
 * See the header file for an explanation of why.
 */
#include "StdAfx.h"
#ifdef _WIN32
#include "DiskImgPriv.h"
#include "SCSIDefs.h"
#include "ASPI.h"
#include "SPTI.h"


/*
 * This is an ugly hack until I can figure out the right way to do this.  To
 * help prevent people from trashing their Windows volumes, we refuse to
 * open "C:\" or physical0 for writing.  On most systems, this is fine.
 *
 * The problem is that, on some systems with a mix of SATA and IDE devices,
 * the boot disk is not physical disk 0.  There should be a way to determine
 * which physical disk is used for booting, or which physical disk
 * corresponds to "C:", but I haven't been able to find it.
 *
 * So, for now, allow a global setting that disables the protection.  I'm
 * doing it this way rather than passing a parameter through because it
 * requires adding an argument to several layers of "open", and I'm hoping
 * to make this go away.
 */
//extern bool DiskImgLib::gAllowWritePhys0;


/*
 * ===========================================================================
 *      Win32VolumeAccess
 * ===========================================================================
 */

/*
 * Open a logical volume.
 */
DIError Win32VolumeAccess::Open(const WCHAR* deviceName, bool readOnly)
{
    DIError dierr = kDIErrNone;

    assert(deviceName != NULL);

    if (fpBlockAccess != NULL) {
        assert(false);
        return kDIErrAlreadyOpen;
    }

#ifdef WANT_ASPI
    if (strncmp(deviceName, kASPIDev, strlen(kASPIDev)) == 0) {
        fpBlockAccess = new ASPIBlockAccess;
        if (fpBlockAccess == NULL) {
            dierr = kDIErrMalloc;
            goto bail;
        }
        dierr = fpBlockAccess->Open(deviceName, readOnly);
        if (dierr != kDIErrNone)
            goto bail;
    } else
#endif
    if (deviceName[0] >= 'A' && deviceName[0] <= 'Z') {
        fpBlockAccess = new LogicalBlockAccess;
        if (fpBlockAccess == NULL) {
            dierr = kDIErrMalloc;
            goto bail;
        }
        dierr = fpBlockAccess->Open(deviceName, readOnly);
        if (dierr != kDIErrNone)
            goto bail;
    } else if (deviceName[0] >= '0' && deviceName[0] <= '9') {
        fpBlockAccess = new PhysicalBlockAccess;
        if (fpBlockAccess == NULL) {
            dierr = kDIErrMalloc;
            goto bail;
        }
        dierr = fpBlockAccess->Open(deviceName, readOnly);
        if (dierr != kDIErrNone)
            goto bail;
    } else {
        LOGI(" Win32VA: '%s' isn't the name of a device", deviceName);
        return kDIErrInternal;
    }

    // Need to do this now so we can use floppy geometry.
    dierr = fpBlockAccess->DetectCapacity(&fTotalBlocks);
    if (dierr != kDIErrNone)
        goto bail;
    assert(fTotalBlocks >= 0);

bail:
    if (dierr != kDIErrNone) {
        delete fpBlockAccess;
        fpBlockAccess = NULL;
    }
    return dierr;
}

/*
 * Close the device.
 */
void Win32VolumeAccess::Close(void)
{
    if (fpBlockAccess != NULL) {
        DIError dierr;
        LOGI("  Win32VolumeAccess closing");

        dierr = FlushCache(true);
        if (dierr != kDIErrNone) {
            LOGI("WARNING: Win32VA Close: FlushCache failed (err=%ld)",
                dierr);
            // not much we can do
        }
        dierr = fpBlockAccess->Close();
        if (dierr != kDIErrNone) {
            LOGI("WARNING: Win32VolumeAccess BlockAccess Close failed (err=%ld)",
                dierr);
        }
        delete fpBlockAccess;
        fpBlockAccess = NULL;
    }
}

/*
 * Read a range of blocks from the device.
 *
 * Because some things like to read partial blocks, we cache the last block
 * we read whenever the caller asks for a single block.  This results in
 * increased data copying, but since we know we're reading from a logical
 * volume it's safe to assume that memory is *many* times faster than
 * reading from this handle.
 *
 * Returns with an error if any of the blocks could not be read.
 */
DIError Win32VolumeAccess::ReadBlocks(long startBlock, short blockCount,
    void* buf)
{
    DIError dierr = kDIErrNone;

    assert(startBlock >= 0);
    assert(blockCount > 0);
    assert(buf != NULL);
    assert(fpBlockAccess != NULL);

    if (blockCount == 1) {
        if (fBlockCache.IsBlockInCache(startBlock)) {
            fBlockCache.GetFromCache(startBlock, buf);
            return kDIErrNone;
        }
    } else {
        // If they're reading in large stretches, we don't need to use
        // the cache.  There is some chance that what we're about to
        // read might include dirty blocks currently in the cache, so
        // we flush what we have before the read.
        dierr = FlushCache(true);
        if (dierr != kDIErrNone)
            return dierr;
    }

    /* go read from the volume */
    dierr = fpBlockAccess->ReadBlocks(startBlock, blockCount, buf);
    if (dierr != kDIErrNone)
        goto bail;


    /* if we're doing single-block reads, put it in the cache */
    if (blockCount == 1) {
        if (!fBlockCache.IsRoomInCache(startBlock)) {
            dierr = FlushCache(true);       // make room
            if (dierr != kDIErrNone)
                goto bail;
        }

        // after flushing, this should never fail
        dierr = fBlockCache.PutInCache(startBlock, buf, false);
        if (dierr != kDIErrNone)
            goto bail;
    }

bail:
    return dierr;
}

/*
 * Write a range of blocks to the device.  For the most part this just
 * writes to the cache.
 *
 * Returns with an error if any of the blocks could not be read.
 */
DIError Win32VolumeAccess::WriteBlocks(long startBlock, short blockCount,
    const void* buf)
{
    DIError dierr = kDIErrNone;

    assert(startBlock >= 0);
    assert(blockCount > 0);
    assert(buf != NULL);

    if (blockCount == 1) {
        /* is this block already in the cache? */
        if (!fBlockCache.IsBlockInCache(startBlock)) {
            /* not present, make sure it fits */
            if (!fBlockCache.IsRoomInCache(startBlock)) {
                dierr = FlushCache(true);       // make room
                if (dierr != kDIErrNone)
                    goto bail;
            }
        }

        // after flushing, this should never fail
        dierr = fBlockCache.PutInCache(startBlock, buf, true);
        if (dierr != kDIErrNone)
            goto bail;
    } else {
        // If they're writing in large stretches, we don't need to use
        // the cache.  We need to flush the cache in case what we're about
        // to write would overwrite something in the cache -- if we don't,
        // what's in the cache will effectively revert what we're about to
        // write.
        dierr = FlushCache(true);
        if (dierr != kDIErrNone)
            goto bail;
        dierr = DoWriteBlocks(startBlock, blockCount, buf);
        if (dierr != kDIErrNone)
            goto bail;
    }

bail:
    return dierr;
}

/*
 * Write all blocks in the cache to disk if any of them are dirty.  In some
 * ways this is wasteful -- we could be writing stuff that isn't dirty,
 * which isn't a great idea on (say) a CF volume -- but in practice we
 * don't mix a lot of adjacent reads and writes, so this is pretty
 * harmless.
 *
 * The goal was to write whole tracks on floppies.  It's easy enough to
 * disable the cache for CF devices if need be.
 *
 * If "purge" is set, we discard the blocks after writing them.
 */
DIError Win32VolumeAccess::FlushCache(bool purge)
{
    DIError dierr = kDIErrNone;

    //LOGI("  Win32VA: FlushCache (%d)", purge);

    if (fBlockCache.IsDirty()) {
        long firstBlock;
        int numBlocks;
        void* buf;

        fBlockCache.GetCachePointer(&firstBlock, &numBlocks, &buf);

        LOGI("FlushCache writing first=%d num=%d (purge=%d)",
            firstBlock, numBlocks, purge);
        dierr = DoWriteBlocks(firstBlock, numBlocks, buf);
        if (dierr != kDIErrNone) {
            LOGI("  Win32VA: FlushCache write blocks failed (err=%d)",
                dierr);
            goto bail;
        }

        // all written, clear the dirty flags
        fBlockCache.Scrub();
    }

    if (purge)
        fBlockCache.Purge();

bail:
    return dierr;
}


/*
 * ===========================================================================
 *      BlockAccess
 * ===========================================================================
 */

/*
 * Detect the capacity of a drive using the SCSI READ CAPACITY command.  Only
 * works on CD-ROM drives and SCSI devices.
 *
 * Unfortunately, if you're accessing a hard drive through the BIOS, SPTI
 * doesn't work.  There must be a better way.
 *
 * On success, "*pNumBlocks" gets the number of 512-byte blocks.
 */
DIError Win32VolumeAccess::BlockAccess::DetectCapacitySPTI(HANDLE handle,
    bool isCDROM, long* pNumBlocks)
{
#ifndef HAVE_WINDOWS_CDROM
    if (isCDROM)
        return kDIErrCDROMNotSupported;
#endif

    DIError dierr = kDIErrNone;
    uint32_t lba, blockLen;

    dierr = SPTI::GetDeviceCapacity(handle, &lba, &blockLen);
    if (dierr != kDIErrNone)
        goto bail;

    LOGI("READ CAPACITY reports lba=%u blockLen=%u (total=%u)",
        lba, blockLen, lba*blockLen);

    if (isCDROM && blockLen != kCDROMSectorSize) {
        LOGW("Unacceptable CD-ROM blockLen=%ld, bailing", blockLen);
        dierr = kDIErrReadFailed;
        goto bail;
    }

    // The LBA is the last valid block on the disk.  To get the disk size,
    // we need to add one.

    *pNumBlocks = (blockLen/512) * (lba+1);
    LOGI(" SPTI returning 512-byte block count %ld", *pNumBlocks);

bail:
    return dierr;
}

/*
 * Figure out how large this disk volume is by probing for readable blocks.
 * We take some guesses for common sizes and then binary-search if necessary.
 *
 * CF cards typically don't have as much space as they're rated for, possibly
 * because of bad-block mapping (either bad blocks or space reserved for when
 * blocks do go bad).
 *
 * This sets "*pNumBlocks" on success.  The largest size this will detect
 * is currently 8GB.
 */
/*static*/ DIError Win32VolumeAccess::BlockAccess::ScanCapacity(BlockAccess* pThis,
    long* pNumBlocks)
{
    DIError dierr = kDIErrNone;
    // max out at 8GB (-1 block)
    const long kLargest = DiskImgLib::kVolumeMaxBlocks;
    const long kTypicalSizes[] = {
        // these must be in ascending order
        720*1024 / BlockAccess::kBlockSize,         // 720K floppy
        1440*1024 / BlockAccess::kBlockSize,        // 1.44MB floppy
        32*1024*1024 / BlockAccess::kBlockSize,     // 32MB flash card
        64*1024*1024 / BlockAccess::kBlockSize,     // 64MB flash card
        128*1024*1024 / BlockAccess::kBlockSize,    // 128MB flash card
        256*1024*1024 / BlockAccess::kBlockSize,    // 256MB flash card
        //512*1024*1024 / BlockAccess::kBlockSize,  // 512MB flash card
        2*1024*1024*(1024/BlockAccess::kBlockSize), // 2GB mark
        kLargest
    };
    long totalBlocks = 0;
    int i;


    // Trivial check to make sure *anything* works, and to establish block 0
    // as valid in case we have to bin-search.
    if (!CanReadBlock(pThis, 0)) {
        LOGI("  Win32VolumeAccess: can't read block 0");
        dierr = kDIErrReadFailed;
        goto bail;
    }

    for (i = 0; i < NELEM(kTypicalSizes); i++) {
        if (!CanReadBlock(pThis, kTypicalSizes[i])) {
            /* failed reading, see if N-1 is readable */
            if (CanReadBlock(pThis, kTypicalSizes[i] - 1)) {
                /* found it */
                totalBlocks = kTypicalSizes[i];
                break;
            } else {
                /* we overshot, binary-search backwards */
                LOGI("OVERSHOT at %ld", kTypicalSizes[i]);
                long good, bad;
                if (i == 0)
                    good = 0;
                else
                    good = kTypicalSizes[i-1];  // know this one is good
                bad = kTypicalSizes[i]-1;       // know this one is bad

                while (good != bad-1) {
                    long check = (good + bad) / 2;
                    assert(check > good);
                    assert(check < bad);

                    if (CanReadBlock(pThis, check))
                        good = check;
                    else
                        bad = check;
                }
                totalBlocks = bad;
                break;
            }
        }
    }
    if (totalBlocks == 0) {
        if (i == NELEM(kTypicalSizes)) {
            /* huge volume, we never found a bad block */
            totalBlocks = kLargest;
        } else {
            /* we never found a good block */
            LOGI("  Win32VolumeAccess unable to determine size");
            dierr = kDIErrReadFailed;
            goto bail;
        }
    }

    if (totalBlocks > (3 * 1024 * 1024) / BlockAccess::kBlockSize) {
        LOGI("  GFDWinVolume: size is %ld (%.2fMB)",
            totalBlocks, (float) totalBlocks / (2048.0));
    } else {
        LOGI("  GFDWinVolume: size is %ld (%.2fKB)",
            totalBlocks, (float) totalBlocks / 2.0);
    }
    *pNumBlocks = totalBlocks;

bail:
    return dierr;
}

/*
 * Figure out if the block at "blockNum" exists.
 */
/*static*/ bool Win32VolumeAccess::BlockAccess::CanReadBlock(BlockAccess* pThis,
    long blockNum)
{
    DIError dierr;
    uint8_t buf[BlockAccess::kBlockSize];

    dierr = pThis->ReadBlocks(blockNum, 1, buf);
    if (dierr == kDIErrNone) {
        LOGI("    +++ Checked %ld (good)", blockNum);
        return true;
    } else {
        LOGI("    +++ Checked %ld (bad)", blockNum);
        return false;
    }
}


#ifndef INVALID_SET_FILE_POINTER
# define INVALID_SET_FILE_POINTER 0xFFFFFFFF
#endif


/*
 * Most of these definitions come from MSFT knowledge base article #174569,
 * "BUG: Int 21 Read/Write Track on Logical Drive Fails on OSR2 and Later".
 */
#define VWIN32_DIOC_DOS_IOCTL       1       // Int 21h 4400h through 4411h
#define VWIN32_DIOC_DOS_INT25       2
#define VWIN32_DIOC_DOS_INT26       3
#define VWIN32_DIOC_DOS_INT13       4
#define VWIN32_DIOC_DOS_DRIVEINFO   6       // Int 21h 730x commands

typedef struct _DIOC_REGISTERS {
    DWORD reg_EBX;
    DWORD reg_EDX;
    DWORD reg_ECX;
    DWORD reg_EAX;
    DWORD reg_EDI;
    DWORD reg_ESI;
    DWORD reg_Flags;
} DIOC_REGISTERS, *PDIOC_REGISTERS;

#define CARRY_FLAG 1

#pragma pack(1)
typedef struct _DISKIO {
    DWORD  dwStartSector;   // starting logical sector number
    WORD   wSectors;        // number of sectors
    DWORD  dwBuffer;        // address of read/write buffer
} DISKIO, *PDISKIO;
typedef struct _DRIVEMAPINFO {
    BYTE    dmiAllocationLength;
    BYTE    dmiInfoLength;
    BYTE    dmiFlags;
    BYTE    dmiInt13Unit;
    DWORD   dmiAssociatedDriveMap;
    DWORD   dmiPartitionStartRBA_lo;
    DWORD   dmiPartitionStartRBA_hi;
} DRIVEMAPINFO, *PDRIVEMAPINFO;
#pragma pack()

#define kInt13StatusMissingAddrMark 2       // sector number above media format
#define kInt13StatusWriteProtected  3       // disk is write protected
#define kInt13StatusSectorNotFound  4       // sector number above drive cap
// == 10 for bad blocks??
#define kInt13StatusTimeout         128     // drive not responding

#if 0
/*
 * Determine the mapping between a logical device number (A=1, B=2, etc)
 * and the Int13h unit number (floppy=00h, hard drive=80h, etc).
 *
 * Pass in the vwin32 handle.
 */
/*static*/ DIError Win32VolumeAccess::BlockAccess::GetInt13Unit(HANDLE handle,
    int driveNum, int* pInt13Unit)
{
    DIError dierr = kDIErrNone;
    BOOL result;
    DWORD cb;
    DRIVEMAPINFO driveMapInfo = {0};
    DIOC_REGISTERS reg = {0};
    const int kGetDriveMapInfo = 0x6f;
    const int kDeviceCategory1 = 0x08;      // for older stuff
    const int kDeviceCategory2 = 0x48;      // for FAT32

    assert(handle != NULL);
    assert(driveNum > 0 && driveNum <= kNumLogicalVolumes);
    assert(pInt13Unit != NULL);

    *pInt13Unit = -1;

    driveMapInfo.dmiAllocationLength = sizeof(DRIVEMAPINFO);    // should be 16

    reg.reg_EAX = 0x440d;       // generic IOCTL
    reg.reg_EBX = driveNum;
    reg.reg_ECX = MAKEWORD(kGetDriveMapInfo, kDeviceCategory1);
    reg.reg_EDX = (DWORD) &driveMapInfo;

    result = ::DeviceIoControl(handle, VWIN32_DIOC_DOS_IOCTL,
                &reg, sizeof(reg),
                &reg, sizeof(reg), &cb, 0);
    if (result == 0) {
        LOGI("  DeviceIoControl(Int21h, 6fh) FAILED (err=%ld)",
            GetLastError());
        dierr = LastErrorToDIError();
        goto bail;
    }

    if (reg.reg_Flags & CARRY_FLAG) {
        LOGI("  --- retrying GDMI with alternate device category (ax=%ld)",
            reg.reg_EAX);
        reg.reg_EAX = 0x440d;       // generic IOCTL
        reg.reg_EBX = driveNum;
        reg.reg_ECX = MAKEWORD(kGetDriveMapInfo, kDeviceCategory2);
        reg.reg_EDX = (DWORD) &driveMapInfo;
        result = ::DeviceIoControl(handle, VWIN32_DIOC_DOS_IOCTL,
                    &reg, sizeof(reg),
                    &reg, sizeof(reg), &cb, 0);
    }
    if (result == 0) {
        LOGI("  DeviceIoControl(Int21h, 6fh)(retry) FAILED (err=%ld)",
            GetLastError());
        dierr = LastErrorToDIError();
        goto bail;
    }
    if (reg.reg_Flags & CARRY_FLAG) {
        LOGI("  --- GetDriveMapInfo call (retry) failed (ax=%ld)",
            reg.reg_EAX);
        dierr = kDIErrReadFailed;   // close enough
        goto bail;
    }

    LOGI("     +++ DriveMapInfo len=%d flags=%d unit=%d",
        driveMapInfo.dmiInfoLength, driveMapInfo.dmiFlags,
        driveMapInfo.dmiInt13Unit);
    LOGI("     +++   driveMap=0x%08lx RBA=0x%08lx 0x%08lx",
        driveMapInfo.dmiAssociatedDriveMap,
        driveMapInfo.dmiPartitionStartRBA_hi,
        driveMapInfo.dmiPartitionStartRBA_lo);

    if (driveMapInfo.dmiInfoLength < 4) {
        /* results not covered in reply? */
        dierr = kDIErrReadFailed;       // not close, but it'll do
        goto bail;
    }

    *pInt13Unit = driveMapInfo.dmiInt13Unit;

bail:
    return dierr;
}
#endif

#if 0
/*
 * Look up the geometry for a floppy disk whose total size is "totalBlocks".
 * There is no BIOS function to detect the media size and geometry, so
 * we have to do it this way.  PC "FAT" disks have a size indication
 * in the boot block, but we can't rely on that.
 *
 * Returns "true" if the geometry is known, "false" otherwise.  When "true"
 * is returned, "*pNumTracks", "*pNumHeads", and "*pNumSectors" will receive
 * values if the pointers are non-NULL.
 */
/*static*/ bool Win32VolumeAccess::BlockAccess::LookupFloppyGeometry(long totalBlocks,
    DiskGeometry* pGeometry)
{
    static const struct {
        FloppyKind  kind;
        long        blockCount;     // total #of blocks on the disk
        int         numCyls;        // #of cylinders
        int         numHeads;       // #of heads per cylinder
        int         numSectors;     // #of sectors/track
    } floppyGeometry[] = {
        { kFloppyUnknown,   -1,     -1, -1, -1 },
        { kFloppy525_360,   360*2,  40, 2,  9 },
        { kFloppy525_1200,  1200*2, 80, 2,  15 },
        { kFloppy35_720,    720*2,  80, 2,  9 },
        { kFloppy35_1440,   1440*2, 80, 2,  18 },
        { kFloppy35_2880,   2880*2, 80, 2,  36 }
    };

    /* verify that we can directly index the table with the enum */
    for (int chk = 0; chk < NELEM(floppyGeometry); chk++) {
        assert(floppyGeometry[chk].kind == chk);
    }
    assert(chk == kFloppyMax);

    if (totalBlocks <= 0) {
        // still auto-detecting volume size?
        return false;
    }

    int i;
    for (i = 0; i < NELEM(floppyGeometry); i++)
        if (floppyGeometry[i].blockCount == totalBlocks)
            break;

    if (i == NELEM(floppyGeometry)) {
        LOGI(  "GetFloppyGeometry: no match for blocks=%ld", totalBlocks);
        return false;
    }

    pGeometry->numCyls = floppyGeometry[i].numCyls;
    pGeometry->numHeads = floppyGeometry[i].numHeads;
    pGeometry->numSectors = floppyGeometry[i].numSectors;

    return true;
}
#endif

/*
 * Convert a block number to a cylinder/head/sector offset.  Also figures
 * out what the last block on the current track is.  Sectors are returned
 * in 1-based form.
 *
 * Returns "true" on success, "false" on failure.
 */
/*static*/ bool Win32VolumeAccess::BlockAccess::BlockToCylinderHeadSector(long blockNum,
    const DiskGeometry* pGeometry, int* pCylinder, int* pHead,
    int* pSector, long* pLastBlockOnTrack)
{
    int cylinder, head, sector;
    long lastBlockOnTrack;
    int leftOver;

    cylinder = blockNum / (pGeometry->numSectors * pGeometry->numHeads);
    leftOver = blockNum - cylinder * (pGeometry->numSectors * pGeometry->numHeads);
    head = leftOver / pGeometry->numSectors;
    sector = leftOver - (head * pGeometry->numSectors);

    assert(cylinder >= 0 && cylinder < pGeometry->numCyls);
    assert(head >= 0 && head < pGeometry->numHeads);
    assert(sector >= 0 && sector < pGeometry->numSectors);

    lastBlockOnTrack = blockNum + (pGeometry->numSectors - sector -1);

    if (pCylinder != NULL)
        *pCylinder = cylinder;
    if (pHead != NULL)
        *pHead = head;
    if (pSector != NULL)
        *pSector = sector+1;
    if (pLastBlockOnTrack != NULL)
        *pLastBlockOnTrack = lastBlockOnTrack;

    return true;
}

/*
 * Get the floppy drive kind (*not* the media kind) using Int13h func 8.
 */
/*static*/ DIError Win32VolumeAccess::BlockAccess::GetFloppyDriveKind(HANDLE handle,
    int unitNum, FloppyKind* pKind)
{
    DIOC_REGISTERS  reg = {0};
    DWORD           cb;
    BOOL            result;

    reg.reg_EAX = MAKEWORD(0, 0x08);    // Read Diskette Drive Parameters
    reg.reg_EDX = MAKEWORD(unitNum, 0);

    result = DeviceIoControl(handle, VWIN32_DIOC_DOS_INT13, &reg,
                sizeof(reg), &reg, sizeof(reg), &cb, 0);

    if (result == 0 || (reg.reg_Flags & CARRY_FLAG)) {
        LOGI(" GetFloppyKind failed: result=%d flags=0x%04x AH=%d",
            result, reg.reg_Flags, HIBYTE(reg.reg_EAX));
        return kDIErrGeneric;
    }

    int bl = LOBYTE(reg.reg_EBX);
    if (bl > 0 && bl < 6)
        *pKind = (FloppyKind) bl;
    else {
        LOGI(" GetFloppyKind: unrecognized kind %d", bl);
        return kDIErrGeneric;
    }

    return kDIErrNone;
}

/*
 * Read one or more blocks using Int13h services.  This only works on
 * floppy drives due to Win9x limitations.
 *
 * The average BIOS will only read one or two tracks reliably, and may
 * not work well when straddling tracks.  It's up to the caller to
 * ensure that the parameters are set properly.
 *
 * "cylinder" and "head" are 0-based, "sector" is 1-based.
 *
 * Returns 0 on success, the status code from AH on failure.  If the call
 * fails but AH is zero, -1 is returned.
 */
/*static*/ int Win32VolumeAccess::BlockAccess::ReadBlocksInt13h(HANDLE handle,
    int unitNum, int cylinder, int head, int sector, short blockCount, void* buf)
{
    DIOC_REGISTERS  reg = {0};
    DWORD           cb;
    BOOL            result;

    if (unitNum < 0 || unitNum >= 4) {
        assert(false);
        return kDIErrInternal;
    }

    for (int retry = 0; retry < kMaxFloppyRetries; retry++) {
        reg.reg_EAX = MAKEWORD(blockCount, 0x02);   // read N sectors
        reg.reg_EBX = (DWORD) buf;
        reg.reg_ECX = MAKEWORD(sector, cylinder);
        reg.reg_EDX = MAKEWORD(unitNum, head);

        //LOGI("   DIOC Int13h read c=%d h=%d s=%d rc=%d",
        //  cylinder, head, sector, blockCount);
        result = DeviceIoControl(handle, VWIN32_DIOC_DOS_INT13, &reg,
                    sizeof(reg), &reg, sizeof(reg), &cb, 0);

        if (result != 0 && !(reg.reg_Flags & CARRY_FLAG))
            break;  // success!

        /* if it's an invalid sector request, bail out immediately */
        if (HIBYTE(reg.reg_EAX) == kInt13StatusSectorNotFound ||
            HIBYTE(reg.reg_EAX) == kInt13StatusMissingAddrMark)
        {
            break;
        }
        LOGI("   DIOC soft read failure, ax=0x%08lx", reg.reg_EAX);
    }
    if (!result || (reg.reg_Flags & CARRY_FLAG)) {
        int ah = HIBYTE(reg.reg_EAX);
        LOGI("  DIOC read failed, result=%d ah=%ld", result, ah);
        if (ah != 0)
            return ah;
        else
            return -1;
    }

    return 0;
}

/*
 * Read one or more blocks using Int13h services.  This only works on
 * floppy drives due to Win9x limitations.
 *
 * It's important to be able to read multiple blocks for performance
 * reasons.  Because this is fairly "raw", we have to retry it 3x before
 * giving up.
 */
/*static*/ DIError Win32VolumeAccess::BlockAccess::ReadBlocksInt13h(HANDLE handle,
    int unitNum, const DiskGeometry* pGeometry, long startBlock, short blockCount,
    void* buf)
{
    int cylinder, head, sector;
    int status, runCount;
    long lastBlockOnTrack;

    if (unitNum < 0 || unitNum >= 4) {
        assert(false);
        return kDIErrInternal;
    }
    if (startBlock < 0 || blockCount <= 0)
        return kDIErrInvalidArg;
    if (startBlock + blockCount > pGeometry->blockCount) {
        LOGI(" ReadInt13h: invalid request for start=%ld count=%d",
            startBlock, blockCount);
        return kDIErrReadFailed;
    }

    while (blockCount) {
        int result;
        result = BlockToCylinderHeadSector(startBlock, pGeometry,
                    &cylinder, &head, &sector, &lastBlockOnTrack);
        assert(result);

        /*
         * According to "The Undocumented PC", the average BIOS will read
         * at most one or two tracks reliably.  It's really geared toward
         * writing a single track or cylinder with one call.  We want to
         * be sure that our read doesn't straddle tracks, so we break it
         * down as needed.
         */
        runCount = lastBlockOnTrack - startBlock +1;
        if (runCount > blockCount)
            runCount = blockCount;
        //LOGI("R runCount=%d lastBlkOnT=%d start=%d",
        //  runCount, lastBlockOnTrack, startBlock);
        assert(runCount > 0);

        status = ReadBlocksInt13h(handle, unitNum, cylinder, head, sector,
                    runCount, buf);
        if (status != 0) {
            LOGI("  DIOC read failed (status=%d)", status);
            return kDIErrReadFailed;
        }

        startBlock += runCount;
        blockCount -= runCount;
    }

    return kDIErrNone;
}

/*
 * Write one or more blocks using Int13h services.  This only works on
 * floppy drives due to Win9x limitations.
 *
 * "cylinder" and "head" are 0-based, "sector" is 1-based.
 *
 * It's important to be able to write multiple blocks for performance
 * reasons.  Because this is fairly "raw", we have to retry it 3x before
 * giving up.
 */
/*static*/ int Win32VolumeAccess::BlockAccess::WriteBlocksInt13h(HANDLE handle,
    int unitNum, int cylinder, int head, int sector, short blockCount,
    const void* buf)
{
    DIOC_REGISTERS  reg = {0};
    DWORD           cb;
    BOOL            result;

    for (int retry = 0; retry < kMaxFloppyRetries; retry++) {
        reg.reg_EAX = MAKEWORD(blockCount, 0x03);   // write N sectors
        reg.reg_EBX = (DWORD) buf;
        reg.reg_ECX = MAKEWORD(sector, cylinder);
        reg.reg_EDX = MAKEWORD(unitNum, head);

        LOGI("   DIOC Int13h write c=%d h=%d s=%d rc=%d",
            cylinder, head, sector, blockCount);
        result = DeviceIoControl(handle, VWIN32_DIOC_DOS_INT13, &reg,
                    sizeof(reg), &reg, sizeof(reg), &cb, 0);

        if (result != 0 && !(reg.reg_Flags & CARRY_FLAG))
            break;  // success!

        if (HIBYTE(reg.reg_EAX) == kInt13StatusWriteProtected)
            break;  // no point retrying this
        LOGI("    DIOC soft write failure, ax=0x%08lx", reg.reg_EAX);
    }
    if (!result || (reg.reg_Flags & CARRY_FLAG)) {
        int ah = HIBYTE(reg.reg_EAX);
        LOGI("  DIOC write failed, result=%d ah=%ld", result, ah);
        if (ah != 0)
            return ah;
        else
            return -1;
    }

    return 0;
}

/*
 * Write one or more blocks using Int13h services.  This only works on
 * floppy drives.
 *
 * It's important to be able to write multiple blocks for performance
 * reasons.  Because this is fairly "raw", we have to retry it 3x before
 * giving up.
 *
 * Returns "true" on success, "false" on failure.
 */
/*static*/ DIError Win32VolumeAccess::BlockAccess::WriteBlocksInt13h(HANDLE handle,
    int unitNum, const DiskGeometry* pGeometry, long startBlock, short blockCount,
    const void* buf)
{
    int cylinder, head, sector;
    int status, runCount;
    long lastBlockOnTrack;

    // make sure this is a floppy drive and we know which unit it is
    if (unitNum < 0 || unitNum >= 4) {
        assert(false);
        return kDIErrInternal;
    }
    if (startBlock < 0 || blockCount <= 0)
        return kDIErrInvalidArg;
    if (startBlock + blockCount > pGeometry->blockCount) {
        LOGI(" WriteInt13h: invalid request for start=%ld count=%d",
            startBlock, blockCount);
        return kDIErrWriteFailed;
    }

    while (blockCount) {
        int result;
        result = BlockToCylinderHeadSector(startBlock, pGeometry,
                    &cylinder, &head, &sector, &lastBlockOnTrack);
        assert(result);

        runCount = lastBlockOnTrack - startBlock +1;
        if (runCount > blockCount)
            runCount = blockCount;
        //LOGI("W runCount=%d lastBlkOnT=%d start=%d",
        //  runCount, lastBlockOnTrack, startBlock);
        assert(runCount > 0);

        status = WriteBlocksInt13h(handle, unitNum, cylinder, head, sector,
                    runCount, buf);
        if (status != 0) {
            LOGI("  DIOC write failed (status=%d)", status);
            if (status == kInt13StatusWriteProtected)
                return kDIErrWriteProtected;
            else
                return kDIErrWriteFailed;
        }

        startBlock += runCount;
        blockCount -= runCount;
    }

    return kDIErrNone;
}

/*
 * Read blocks from a Win9x logical volume, using Int21h func 7305h.  Pass in
 * a handle to vwin32 and the logical drive number (A=1, B=2, etc).
 *
 * Works on Win95 OSR2 and later.  Earlier versions require Int25 or Int13.
 */
/*static*/ DIError Win32VolumeAccess::BlockAccess::ReadBlocksInt21h(HANDLE handle,
    int driveNum, long startBlock, short blockCount, void* buf)
{
#if 0
    assert(false);      // discouraged
    BOOL            result;
    DWORD           cb;
    DIOC_REGISTERS  reg = {0};
    DISKIO          dio = {0};

    dio.dwStartSector = startBlock;
    dio.wSectors      = (WORD) blockCount;
    dio.dwBuffer      = (DWORD) buf;

    reg.reg_EAX = fDriveNum - 1;    // Int 25h drive numbers are 0-based.
    reg.reg_EBX = (DWORD)&dio;
    reg.reg_ECX = 0xFFFF;       // use DISKIO struct

    LOGI("   Int25 read start=%d count=%d",
        startBlock, blockCount);
    result = ::DeviceIoControl(handle, VWIN32_DIOC_DOS_INT25,
                &reg, sizeof(reg),
                &reg, sizeof(reg), &cb, 0);

    // Determine if the DeviceIoControl call and the read succeeded.
    result = result && !(reg.reg_Flags & CARRY_FLAG);

    LOGI("     +++ read from block %ld (result=%d)", startBlock, result);
    if (!result)
        return kDIErrReadFailed;
#else
    BOOL            result;
    DWORD           cb;
    DIOC_REGISTERS  reg = {0};
    DISKIO          dio = {0};

    dio.dwStartSector = startBlock;
    dio.wSectors      = (WORD) blockCount;
    dio.dwBuffer      = (DWORD) buf;

    reg.reg_EAX = 0x7305;   // Ext_ABSDiskReadWrite
    reg.reg_EBX = (DWORD)&dio;
    reg.reg_ECX = -1;
    reg.reg_EDX = driveNum; // Int 21h, fn 7305h drive numbers are 1-based
    assert(reg.reg_ESI == 0);   // read/write flag

    LOGI("   Int21/7305h read start=%d count=%d",
        startBlock, blockCount);
    result = ::DeviceIoControl(handle, VWIN32_DIOC_DOS_DRIVEINFO,
                &reg, sizeof(reg),
                &reg, sizeof(reg), &cb, 0);

    // Determine if the DeviceIoControl call and the read succeeded.
    result = result && !(reg.reg_Flags & CARRY_FLAG);

    LOGI("     +++ RB21h %ld %d (result=%d lastError=%ld)",
        startBlock, blockCount, result, GetLastError());
    if (!result)
        return kDIErrReadFailed;
#endif

    return kDIErrNone;
}

/*
 * Write blocks to a Win9x logical volume.  Pass in a handle to vwin32 and
 * the logical drive number (A=1, B=2, etc).
 *
 * Works on Win95 OSR2 and later.  Earlier versions require Int26 or Int13.
 */
/*static*/ DIError Win32VolumeAccess::BlockAccess::WriteBlocksInt21h(HANDLE handle,
    int driveNum, long startBlock, short blockCount, const void* buf)
{
    BOOL            result;
    DWORD           cb;
    DIOC_REGISTERS  reg = {0};
    DISKIO          dio = {0};

    dio.dwStartSector = startBlock;
    dio.wSectors      = (WORD) blockCount;
    dio.dwBuffer      = (DWORD) buf;

    reg.reg_EAX = 0x7305;   // Ext_ABSDiskReadWrite
    reg.reg_EBX = (DWORD)&dio;
    reg.reg_ECX = -1;
    reg.reg_EDX = driveNum; // Int 21h, fn 7305h drive numbers are 1-based
    reg.reg_ESI = 0x6001;       // write normal data (bit flags)

    //LOGI("   Int21/7305h write start=%d count=%d",
    //  startBlock, blockCount);
    result = ::DeviceIoControl(handle, VWIN32_DIOC_DOS_DRIVEINFO,
                &reg, sizeof(reg),
                &reg, sizeof(reg), &cb, 0);

    // Determine if the DeviceIoControl call and the read succeeded.
    result = result && !(reg.reg_Flags & CARRY_FLAG);

    LOGI("     +++ WB21h %ld %d (result=%d lastError=%ld)",
        startBlock, blockCount, result, GetLastError());
    if (!result)
        return kDIErrWriteFailed;

    return kDIErrNone;
}

/*
 * Read blocks from a Win2K logical or physical volume.
 */
/*static*/ DIError Win32VolumeAccess::BlockAccess::ReadBlocksWin2K(HANDLE handle,
    long startBlock, short blockCount, void* buf)
{
    /*
     * Try to read the blocks.  Under Win2K the seek and read calls appear
     * to succeed, but the value in "actual" is set to zero to indicate
     * that we're trying to read past EOF.
     */
    DWORD posn, actual;

    /*
     * Win2K: the 3rd argument holds the high 32 bits of the distance to
     * move.  This isn't supported in Win9x, which means Win9x is limited
     * to 2GB files.
     */
    LARGE_INTEGER li;
    li.QuadPart = (LONGLONG) startBlock * (LONGLONG) kBlockSize;
    posn = ::SetFilePointer(handle, li.LowPart, &li.HighPart,
                FILE_BEGIN);
    if (posn == INVALID_SET_FILE_POINTER) {
        DWORD lerr = GetLastError();
        LOGI("  GFDWinVolume ReadBlock: SFP failed (err=%ld)", lerr);
        return LastErrorToDIError();
    }

    //LOGI("   ReadFile block start=%d count=%d", startBlock, blockCount);

    BOOL result;
    result = ::ReadFile(handle, buf, blockCount * kBlockSize, &actual, NULL);
    if (!result) {
        DWORD lerr = GetLastError();
        LOGI("  ReadBlocksWin2K: ReadFile failed (start=%ld count=%d err=%ld)",
            startBlock, blockCount, lerr);
        return LastErrorToDIError();
    }
    if ((long) actual != blockCount * kBlockSize)
        return kDIErrEOF;

    return kDIErrNone;
}

/*
 * Write blocks to a Win2K logical or physical volume.
 */
/*static*/ DIError Win32VolumeAccess::BlockAccess::WriteBlocksWin2K(HANDLE handle,
    long startBlock, short blockCount, const void* buf)
{
    DWORD posn, actual;

    posn = ::SetFilePointer(handle, startBlock * kBlockSize, NULL,
                FILE_BEGIN);
    if (posn == INVALID_SET_FILE_POINTER) {
        DWORD lerr = GetLastError();
        LOGI("  GFDWinVolume ReadBlocks: SFP %ld failed (err=%ld)",
            startBlock * kBlockSize, lerr);
        return LastErrorToDIError();
    }

    BOOL result;
    result = ::WriteFile(handle, buf, blockCount * kBlockSize, &actual, NULL);
    if (!result) {
        DWORD lerr = GetLastError();
        LOGI("  GFDWinVolume WriteBlocks: WriteFile failed (err=%ld)",
            lerr);
        return LastErrorToDIError();
    }
    if ((long) actual != blockCount * kBlockSize)
        return kDIErrEOF;       // unexpected on a write call?

    return kDIErrNone;
}


/*
 * ===========================================================================
 *      LogicalBlockAccess
 * ===========================================================================
 */

/*
 * Open a logical device.  The device name should be of the form "A:\".
 */
DIError Win32VolumeAccess::LogicalBlockAccess::Open(const WCHAR* deviceName,
    bool readOnly)
{
    DIError dierr = kDIErrNone;
    const bool kPreferASPI = true;

    assert(fHandle == NULL);
    fIsCDROM = false;
    fDriveNum = -1;

    if (deviceName[0] < 'A' || deviceName[0] > 'Z' ||
        deviceName[1] != ':' || deviceName[2] != '\\' ||
        deviceName[3] != '\0')
    {
        LOGI("  LogicalBlockAccess: invalid device name '%s'", deviceName);
        assert(false);
        dierr = kDIErrInvalidArg;
        goto bail;
    }
    if (deviceName[0] == 'C') {
        if (readOnly == false) {
            LOGI("  REFUSING WRITE ACCESS TO C:\\ ");
            return kDIErrVWAccessForbidden;
        }
    }

    DWORD access;
    if (readOnly)
        access = GENERIC_READ;
    else
        access = GENERIC_READ | GENERIC_WRITE;

    UINT driveType;
    driveType = GetDriveType(deviceName);
    if (driveType == DRIVE_CDROM) {
        if (!Global::GetHasSPTI() && !Global::GetHasASPI())
            return kDIErrCDROMNotSupported;

        fIsCDROM = true;
        // SPTI needs this -- maybe to enforce exclusive access?
        access |= GENERIC_WRITE;
    }

    if (fIsWin9x) {
        if (fIsCDROM)
            return kDIErrCDROMNotSupported;

        fHandle = CreateFile(_T("\\\\.\\vwin32"), 0, 0, NULL,
                    OPEN_EXISTING, FILE_FLAG_DELETE_ON_CLOSE, NULL);
        if (fHandle == INVALID_HANDLE_VALUE) {
            DWORD lastError = GetLastError();
            LOGI("  Win32LVOpen: CreateFile(vwin32) failed (err=%ld)",
                lastError);
            dierr = LastErrorToDIError();
            goto bail;
        }
        fDriveNum = deviceName[0] - 'A' +1;
        assert(fDriveNum > 0 && fDriveNum <= kNumLogicalVolumes);

#if 0
        int int13Unit;
        dierr = GetInt13Unit(fHandle, fDriveNum, &int13Unit);
        if (dierr != kDIErrNone)
            goto bail;
        if (int13Unit < 4) {
            fFloppyUnit = int13Unit;
            LOGI(" Logical volume #%d looks like floppy unit %d",
                fDriveNum, fFloppyUnit);
        }
#endif
    } else {
        WCHAR device[7] = _T("\\\\.\\_:");
        device[4] = deviceName[0];
        LOGI("Opening '%s'", device);

        // If we're reading, allow others to write.  If we're writing, insist
        // upon exclusive access to the volume.
        DWORD shareMode = FILE_SHARE_READ;
        if (access == GENERIC_READ)
            shareMode |= FILE_SHARE_WRITE;

        fHandle = CreateFile(device, access, shareMode,
                    NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (fHandle == INVALID_HANDLE_VALUE) {
            DWORD lastError = GetLastError();
            dierr = LastErrorToDIError();
            if (lastError == ERROR_INVALID_PARAMETER && shareMode == FILE_SHARE_READ)
            {
                // Win2K spits this back if the volume is open and we're
                // not specifying FILE_SHARE_WRITE.  Give it a try, just to
                // see if it works, so we can tell the difference between
                // an internal library error and an active filesystem.
                HANDLE tmpHandle;
                tmpHandle = CreateFile(device, access, FILE_SHARE_READ|FILE_SHARE_WRITE,
                            NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
                if (tmpHandle != INVALID_HANDLE_VALUE) {
                    dierr = kDIErrNoExclusiveAccess;
                    ::CloseHandle(tmpHandle);
                }
            }
            LOGI("  LBAccess Open: CreateFile failed (err=%ld dierr=%d)",
                lastError, dierr);
            goto bail;
        }
    }

    assert(fHandle != NULL && fHandle != INVALID_HANDLE_VALUE);

#if 0
    if (fIsCDROM) {
        assert(Global::GetHasSPTI() || Global::GetHasASPI());
        if (Global::GetHasASPI() && (!Global::GetHasSPTI() || kPreferASPI)) {
            LOGI("  LBAccess using ASPI");
            fCDBaggage.fUseASPI = true;
        } else {
            LOGI("  LBAccess using SPTI");
            fCDBaggage.fUseASPI = false;
        }

        if (fCDBaggage.fUseASPI) {
            dierr = FindASPIDriveMapping(deviceName[0]);
            if (dierr != kDIErrNone) {
                LOGI("ERROR: couldn't find ASPI drive mapping");
                dierr = kDIErrNoASPIMapping;
                goto bail;
            }
        }
    }
#endif

bail:
    if (dierr != kDIErrNone) {
        if (fHandle != NULL && fHandle != INVALID_HANDLE_VALUE)
            ::CloseHandle(fHandle);
        fHandle = NULL;
    }
    return dierr;
}

/*
 * Close the device handle.
 */
DIError Win32VolumeAccess::LogicalBlockAccess::Close(void)
{
    if (fHandle != NULL) {
        ::CloseHandle(fHandle);
        fHandle = NULL;
    }
    return kDIErrNone;
}

/*
 * Read 512-byte blocks from CD-ROM media using SPTI calls.
 */
DIError Win32VolumeAccess::LogicalBlockAccess::ReadBlocksCDROM(HANDLE handle,
    long startBlock, short blockCount, void* buf)
{
#ifdef HAVE_WINDOWS_CDROM
    DIError dierr;

    assert(handle != NULL);
    assert(startBlock >= 0);
    assert(blockCount > 0);
    assert(buf != NULL);

    //LOGI(" CDROM read block %ld (%ld)", startBlock, block);

    /* alloc sector buffer on first use */
    if (fLastSectorCache == NULL) {
        fLastSectorCache = new uint8_t[kCDROMSectorSize];
        if (fLastSectorCache == NULL)
            return kDIErrMalloc;
        assert(fLastSectorNum == -1);
    }

    /*
     * Map a range of 512-byte blocks to a range of 2048-byte blocks.
     */
    assert(kCDROMSectorSize % kBlockSize == 0);
    const int kFactor = kCDROMSectorSize / kBlockSize;
    long sectorIndex = startBlock / kFactor;
    int sectorOffset = (int) (startBlock % kFactor);    // 0-3

    /*
     * When possible, do multi-block reads directly into "buf".  The first
     * and last block may require special handling.
     */
    while (blockCount) {
        assert(blockCount > 0);

        if (sectorOffset != 0 || blockCount < kFactor) {
            assert(sectorOffset >= 0 && sectorOffset < kFactor);

            /* get from single-block cache or from disc */
            if (sectorIndex != fLastSectorNum) {
                fLastSectorNum = -1;        // invalidate, in case of error

                dierr = SPTI::ReadBlocks(handle, sectorIndex, 1,
                            kCDROMSectorSize, fLastSectorCache);
                if (dierr != kDIErrNone)
                    return dierr;

                fLastSectorNum = sectorIndex;
            }

            int thisNumBlocks;
            thisNumBlocks = kFactor - sectorOffset;
            if (thisNumBlocks > blockCount)
                thisNumBlocks = blockCount;

            //LOGI("  Small copy (sectIdx=%ld off=%d*512 size=%d*512)",
            //  sectorIndex, sectorOffset, thisNumBlocks);
            memcpy(buf, fLastSectorCache + sectorOffset * kBlockSize,
                thisNumBlocks * kBlockSize);

            blockCount -= thisNumBlocks;
            buf = (uint8_t*) buf + (thisNumBlocks * kBlockSize);

            sectorOffset = 0;
            sectorIndex++;
        } else {
            fLastSectorNum = -1;        // invalidate single-block cache

            int numSectors;
            numSectors = blockCount / kFactor;  // rounds down

            //LOGI("  Big read (sectIdx=%ld numSect=%d)",
            //  sectorIndex, numSectors);
            dierr = SPTI::ReadBlocks(handle, sectorIndex, numSectors,
                        kCDROMSectorSize, buf);
            if (dierr != kDIErrNone)
                return dierr;

            blockCount -= numSectors * kFactor;
            buf = (uint8_t*) buf + (numSectors * kCDROMSectorSize);

            sectorIndex += numSectors;
        }
    }

    return kDIErrNone;

#else
    return kDIErrCDROMNotSupported;
#endif
}


/*
 * ===========================================================================
 *      PhysicalBlockAccess
 * ===========================================================================
 */

/*
 * Open a physical device.  The device name should be of the form "80:\".
 */
DIError Win32VolumeAccess::PhysicalBlockAccess::Open(const WCHAR* deviceName,
    bool readOnly)
{
    DIError dierr = kDIErrNone;

    // initialize all local state
    assert(fHandle == NULL);
    fInt13Unit = -1;
    fFloppyKind = kFloppyUnknown;
    memset(&fGeometry, 0, sizeof(fGeometry));

    // sanity-check name; not this only works for first 10 devices
    if (deviceName[0] < '0' || deviceName[0] > '9' ||
        deviceName[1] < '0' || deviceName[1] > '9' ||
        deviceName[2] != ':' || deviceName[3] != '\\' ||
        deviceName[4] != '\0')
    {
        LOGI("  PhysicalBlockAccess: invalid device name '%s'", deviceName);
        assert(false);
        dierr = kDIErrInvalidArg;
        goto bail;
    }

    if (deviceName[0] == '8' && deviceName[1] == '0') {
        if (!gAllowWritePhys0 && readOnly == false) {
            LOGI("  REFUSING WRITE ACCESS TO 80:\\ ");
            return kDIErrVWAccessForbidden;
        }
    }

    fInt13Unit = (deviceName[0] - '0') * 16 + deviceName[1] - '0';
    if (!fIsWin9x && fInt13Unit < 0x80) {
        LOGI("GLITCH: can't open floppy as physical unit in Win2K");
        dierr = kDIErrInvalidArg;
        goto bail;
    }
    if (fIsWin9x && fInt13Unit >= 0x80) {
        LOGI("GLITCH: can't access physical HD in Win9x");
        dierr = kDIErrInvalidArg;
        goto bail;
    }
    if ((fInt13Unit >= 0x00 && fInt13Unit < 0x04) ||
        (fInt13Unit >= 0x80 && fInt13Unit < 0x88))
    {
        LOGI("  Win32VA/P: opening unit %02xh", fInt13Unit);
    } else {
        LOGI("GLITCH: converted '%s' to %02xh", deviceName, fInt13Unit);
        dierr = kDIErrInternal;
        goto bail;
    }

    DWORD access;
    if (readOnly)
        access = GENERIC_READ;
    else
        access = GENERIC_READ | GENERIC_WRITE;

    if (fIsWin9x) {
        fHandle = CreateFile(_T("\\\\.\\vwin32"), 0, 0, NULL,
                    OPEN_EXISTING, FILE_FLAG_DELETE_ON_CLOSE, NULL);
        if (fHandle == INVALID_HANDLE_VALUE) {
            DWORD lastError = GetLastError();
            LOGI("  Win32VA/PBOpen: CreateFile(vwin32) failed (err=%ld)",
                lastError);
            dierr = LastErrorToDIError();
            goto bail;
        }

        /* figure out the geometry */
        dierr = DetectFloppyGeometry();
        if (dierr != kDIErrNone)
            goto bail;
    } else {
        WCHAR device[19] = _T("\\\\.\\PhysicalDrive_");
        assert(fInt13Unit >= 0x80 && fInt13Unit <= 0x89);
        device[17] = fInt13Unit - 0x80 + '0';
        LOGI("Opening '%s' (access=0x%02x)", device, access);

        // If we're reading, allow others to write.  If we're writing, insist
        // upon exclusive access to the volume.
        DWORD shareMode = FILE_SHARE_READ;
        if (access == GENERIC_READ)
            shareMode |= FILE_SHARE_WRITE;

        fHandle = CreateFile(device, access, shareMode,
                    NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (fHandle == INVALID_HANDLE_VALUE) {
            DWORD lastError = GetLastError();
            dierr = LastErrorToDIError();
            LOGI("  PBAccess Open: CreateFile failed (err=%ld dierr=%d)",
                lastError, dierr);
            goto bail;
        }
    }

    assert(fHandle != NULL && fHandle != INVALID_HANDLE_VALUE);

bail:
    if (dierr != kDIErrNone) {
        if (fHandle != NULL && fHandle != INVALID_HANDLE_VALUE)
            ::CloseHandle(fHandle);
        fHandle = NULL;
    }

    return dierr;
}

/*
 * Auto-detect the geometry of a floppy drive.
 *
 * Sets "fFloppyKind" and "fGeometry".
 */
DIError Win32VolumeAccess::PhysicalBlockAccess::DetectFloppyGeometry(void)
{
    DIError dierr = kDIErrNone;
    static const struct {
        FloppyKind      kind;
        DiskGeometry    geom;
    } floppyGeometry[] = {
        { kFloppyUnknown,   { -1,   -1, -1, -1      } },
        { kFloppy525_360,   { 40,   2,  9,  360*2   } },
        { kFloppy525_1200,  { 80,   2,  15, 1200*2  } },
        { kFloppy35_720,    { 80,   2,  9,  720*2   } },
        { kFloppy35_1440,   { 80,   2,  18, 1440*2  } },
        { kFloppy35_2880,   { 80,   2,  36, 2880*2  } }
    };
    uint8_t buf[kBlockSize];
    FloppyKind driveKind;
    int status;

    /* verify that we can directly index the table with the enum */
    int chk;
    for (chk = 0; chk < NELEM(floppyGeometry); chk++) {
        assert(floppyGeometry[chk].kind == chk);
    }
    assert(chk == kFloppyMax);


    /*
     * Issue a BIOS call to determine the kind of drive we're looking at.
     */
    dierr = GetFloppyDriveKind(fHandle, fInt13Unit, &driveKind);
    if (dierr != kDIErrNone)
        goto bail;

    switch (driveKind) {
    case kFloppy35_2880:
        status = ReadBlocksInt13h(fHandle, fInt13Unit, 0, 0, 36, 1, buf);
        if (status == 0) {
            fFloppyKind = kFloppy35_2880;
            break;
        }
        // else, fall through
    case kFloppy35_1440:
        status = ReadBlocksInt13h(fHandle, fInt13Unit, 0, 0, 18, 1, buf);
        if (status == 0) {
            fFloppyKind = kFloppy35_1440;
            break;
        }
        // else, fall through
    case kFloppy35_720:
        status = ReadBlocksInt13h(fHandle, fInt13Unit, 0, 0, 9, 1, buf);
        if (status == 0) {
            fFloppyKind = kFloppy35_720;
            break;
        }
        // else, fail
        dierr = kDIErrReadFailed;
        goto bail;

    case kFloppy525_1200:
        status = ReadBlocksInt13h(fHandle, fInt13Unit, 0, 0, 15, 1, buf);
        if (status == 0) {
            fFloppyKind = kFloppy525_1200;
            break;
        }
        // else, fall through
    case kFloppy525_360:
        status = ReadBlocksInt13h(fHandle, fInt13Unit, 0, 0, 9, 1, buf);
        if (status == 0) {
            fFloppyKind = kFloppy525_360;
            break;
        }
        // else, fail
        dierr = kDIErrReadFailed;
        goto bail;

    default:
        LOGI(" Unknown driveKind %d", driveKind);
        assert(false);
        dierr = kDIErrInternal;
        goto bail;
    }

    LOGI("PBA: floppy disk appears to be type=%d", fFloppyKind);
    fGeometry = floppyGeometry[fFloppyKind].geom;

bail:
    return dierr;
}

#if 0
/*
 * Flush the system disk cache.
 */
DIError Win32VolumeAccess::PhysicalBlockAccess::FlushBlockDevice(void)
{
    DIError dierr = kDIErrNone;

    if (::FlushFileBuffers(fHandle) == FALSE) {
        DWORD lastError = GetLastError();
        LOGI("  Win32VA/PBAFlush: FlushFileBuffers failed (err=%ld)",
            lastError);
        dierr = LastErrorToDIError();
    }
    return dierr;
}
#endif

/*
 * Close the device handle.
 */
DIError Win32VolumeAccess::PhysicalBlockAccess::Close(void)
{
    if (fHandle != NULL) {
        ::CloseHandle(fHandle);
        fHandle = NULL;
    }
    return kDIErrNone;
}


/*
 * ===========================================================================
 *      ASPIBlockAccess
 * ===========================================================================
 */
#ifdef WANT_ASPI

/*
 * Unpack device name and verify that the device is a CD-ROM drive or
 * direct-access storage device.
 */
DIError Win32VolumeAccess::ASPIBlockAccess::Open(const char* deviceName,
    bool readOnly)
{
    DIError dierr = kDIErrNone;

    if (fpASPI != NULL)
        return kDIErrAlreadyOpen;

    fpASPI = Global::GetASPI();
    if (fpASPI == NULL)
        return kDIErrASPIFailure;

    if (strncmp(deviceName, kASPIDev, strlen(kASPIDev)) != 0) {
        assert(false);
        dierr = kDIErrInternal;
        goto bail;
    }

    const char* cp;
    int adapter, target, lun;
    int result;

    cp = deviceName + strlen(kASPIDev);
    result = 0;
    result |= ExtractInt(&cp, &adapter);
    result |= ExtractInt(&cp, &target);
    result |= ExtractInt(&cp, &lun);
    if (result != 0) {
        LOGI(" Win32VA couldn't parse '%s'", deviceName);
        dierr = kDIErrInternal;
        goto bail;
    }

    fAdapter = adapter;
    fTarget = target;
    fLun = lun;

    uint8_t deviceType;
    dierr = fpASPI->GetDeviceType(fAdapter, fTarget, fLun, &deviceType);
    if (dierr != kDIErrNone ||
        (deviceType != kScsiDevTypeCDROM && deviceType != kScsiDevTypeDASD))
    {
        LOGI(" Win32VA bad GetDeviceType err=%d type=%d",
            dierr, deviceType);
        dierr = kDIErrInternal;     // should not be here at all
        goto bail;
    }
    if (deviceType == kScsiDevTypeCDROM)
        fReadOnly = true;
    else
        fReadOnly = readOnly;

    LOGI(" Win32VA successful 'open' of '%s' on %d:%d:%d",
        deviceName, fAdapter, fTarget, fLun);

bail:
    if (dierr != kDIErrNone)
        fpASPI = NULL;
    return dierr;
}

/*
 * Extract an integer from a string, advancing to the next integer after
 * doing so.
 *
 * Returns 0 on success, -1 on failure.
 */
int Win32VolumeAccess::ASPIBlockAccess::ExtractInt(const char** pStr, int* pResult)
{
    char* end = NULL;

    if (*pStr == NULL) {
        assert(false);
        return -1;
    }

    *pResult = (int) strtol(*pStr, &end, 10);

    if (end == NULL)
        *pStr = NULL;
    else
        *pStr = end+1;

    return 0;
}

/*
 * Return the device capacity in 512-byte blocks.
 *
 * Sets fChunkSize as a side-effect.
 */
DIError Win32VolumeAccess::ASPIBlockAccess::DetectCapacity(long* pNumBlocks)
{
    DIError dierr = kDIErrNone;
    unsigned long lba, blockLen;

    dierr = fpASPI->GetDeviceCapacity(fAdapter, fTarget, fLun, &lba,
                &blockLen);
    if (dierr != kDIErrNone)
        goto bail;

    LOGI("READ CAPACITY reports lba=%lu blockLen=%lu (total=%lu)",
        lba, blockLen, lba*blockLen);

    fChunkSize = blockLen;

    if ((blockLen % 512) != 0) {
        LOGI("Unacceptable CD-ROM blockLen=%ld, bailing", blockLen);
        dierr = kDIErrReadFailed;
        goto bail;
    }

    // The LBA is the last valid block on the disk.  To get the disk size,
    // we need to add one.

    *pNumBlocks = (blockLen/512) * (lba+1);
    LOGI(" ASPI returning 512-byte block count %ld", *pNumBlocks);

bail:
    return dierr;
}

/*
 * Read one or more 512-byte blocks from the device.
 *
 * SCSI doesn't promise it'll be in a chunk size we like, but it's pretty
 * safe to assume that it'll be at least 512 bytes, and divisible by 512.
 */
DIError Win32VolumeAccess::ASPIBlockAccess::ReadBlocks(long startBlock,
    short blockCount, void* buf)
{
    DIError dierr;

    // we're expecting fBlockSize to be 512 or 2048
    assert(fChunkSize >= kBlockSize && fChunkSize <= 65536);
    assert((fChunkSize % kBlockSize) == 0);

    /* alloc chunk buffer on first use */
    if (fLastChunkCache == NULL) {
        fLastChunkCache = new uint8_t[fChunkSize];
        if (fLastChunkCache == NULL)
            return kDIErrMalloc;
        assert(fLastChunkNum == -1);
    }

    /*
     * Map a range of N-byte blocks to a range of 2048-byte blocks.
     */
    const int kFactor = fChunkSize / kBlockSize;
    long chunkIndex = startBlock / kFactor;
    int chunkOffset = (int) (startBlock % kFactor); // small integer

    /*
     * When possible, do multi-block reads directly into "buf".  The first
     * and last block may require special handling.
     */
    while (blockCount) {
        assert(blockCount > 0);

        if (chunkOffset != 0 || blockCount < kFactor) {
            assert(chunkOffset >= 0 && chunkOffset < kFactor);

            /* get from single-block cache or from disc */
            if (chunkIndex != fLastChunkNum) {
                fLastChunkNum = -1;     // invalidate, in case of error

                dierr = fpASPI->ReadBlocks(fAdapter, fTarget, fLun, chunkIndex,
                            1, fChunkSize, fLastChunkCache);
                if (dierr != kDIErrNone)
                    return dierr;

                fLastChunkNum = chunkIndex;
            }

            int thisNumBlocks;
            thisNumBlocks = kFactor - chunkOffset;
            if (thisNumBlocks > blockCount)
                thisNumBlocks = blockCount;

            //LOGI("  Small copy (chIdx=%ld off=%d*512 size=%d*512)",
            //  chunkIndex, chunkOffset, thisNumBlocks);
            memcpy(buf, fLastChunkCache + chunkOffset * kBlockSize,
                thisNumBlocks * kBlockSize);

            blockCount -= thisNumBlocks;
            buf = (uint8_t*) buf + (thisNumBlocks * kBlockSize);

            chunkOffset = 0;
            chunkIndex++;
        } else {
            fLastChunkNum = -1;     // invalidate single-block cache

            int numChunks;
            numChunks = blockCount / kFactor;   // rounds down

            //LOGI("  Big read (chIdx=%ld numCh=%d)",
            //  chunkIndex, numChunks);
            dierr = fpASPI->ReadBlocks(fAdapter, fTarget, fLun, chunkIndex,
                        numChunks, fChunkSize, buf);
            if (dierr != kDIErrNone)
                return dierr;

            blockCount -= numChunks * kFactor;
            buf = (uint8_t*) buf + (numChunks * fChunkSize);

            chunkIndex += numChunks;
        }
    }

    return kDIErrNone;
}

/*
 * Write one or more 512-byte blocks to the device.
 *
 * SCSI doesn't promise it'll be in a chunk size we like, but it's pretty
 * safe to assume that it'll be at least 512 bytes, and divisible by 512.
 */
DIError Win32VolumeAccess::ASPIBlockAccess::WriteBlocks(long startBlock,
    short blockCount, const void* buf)
{
    DIError dierr;

    if (fReadOnly)
        return kDIErrAccessDenied;

    // we're expecting fBlockSize to be 512 or 2048
    assert(fChunkSize >= kBlockSize && fChunkSize <= 65536);
    assert((fChunkSize % kBlockSize) == 0);

    /* throw out the cache */
    fLastChunkNum = -1;

    /*
     * Map a range of N-byte blocks to a range of 2048-byte blocks.
     */
    const int kFactor = fChunkSize / kBlockSize;
    long chunkIndex = startBlock / kFactor;
    int chunkOffset = (int) (startBlock % kFactor); // small integer

    /*
     * When possible, do multi-block writes directly from "buf".  The first
     * and last block may require special handling.
     */
    while (blockCount) {
        assert(blockCount > 0);

        if (chunkOffset != 0 || blockCount < kFactor) {
            assert(chunkOffset >= 0 && chunkOffset < kFactor);

            /* read the chunk we're writing a part of */
            dierr = fpASPI->ReadBlocks(fAdapter, fTarget, fLun, chunkIndex,
                        1, fChunkSize, fLastChunkCache);
            if (dierr != kDIErrNone)
                return dierr;

            int thisNumBlocks;
            thisNumBlocks = kFactor - chunkOffset;
            if (thisNumBlocks > blockCount)
                thisNumBlocks = blockCount;

            LOGI("  Small copy out (chIdx=%ld off=%d*512 size=%d*512)",
                chunkIndex, chunkOffset, thisNumBlocks);
            memcpy(fLastChunkCache + chunkOffset * kBlockSize, buf,
                thisNumBlocks * kBlockSize);

            blockCount -= thisNumBlocks;
            buf = (const uint8_t*) buf + (thisNumBlocks * kBlockSize);

            chunkOffset = 0;
            chunkIndex++;
        } else {
            int numChunks;
            numChunks = blockCount / kFactor;   // rounds down

            LOGI("  Big write (chIdx=%ld numCh=%d)",
                chunkIndex, numChunks);
            dierr = fpASPI->WriteBlocks(fAdapter, fTarget, fLun, chunkIndex,
                        numChunks, fChunkSize, buf);
            if (dierr != kDIErrNone)
                return dierr;

            blockCount -= numChunks * kFactor;
            buf = (const uint8_t*) buf + (numChunks * fChunkSize);

            chunkIndex += numChunks;
        }
    }

    return kDIErrNone;
}

/*
 * Not much to do, really, since we're not holding onto any OS structures.
 */
DIError
Win32VolumeAccess::ASPIBlockAccess::Close(void)
{
    fpASPI = NULL;
    return kDIErrNone;
}
#endif


/*
 * ===========================================================================
 *      CBCache
 * ===========================================================================
 */

/*
 * Determine whether we're holding a block in the cache.
 */
bool CBCache::IsBlockInCache(long blockNum) const
{
    if (fFirstBlock == kEmpty)
        return false;
    assert(fNumBlocks > 0);

    if (blockNum >= fFirstBlock && blockNum < fFirstBlock + fNumBlocks)
        return true;
    else
        return false;
}

/*
 * Retrieve a single block from the cache.
 */
DIError CBCache::GetFromCache(long blockNum, void* buf)
{
    if (!IsBlockInCache(blockNum)) {
        assert(false);
        return kDIErrInternal;
    }

    //LOGI("   CBCache: getting block %d from cache", blockNum);
    int offset = (blockNum - fFirstBlock) * kBlockSize;
    assert(offset >= 0);

    memcpy(buf, fCache + offset, kBlockSize);
    return kDIErrNone;
}

/*
 * Determine whether a block will "fit" in the cache.  There are two
 * criteria: (1) there must actually be room at the end, and (2) the
 * block in question must be the next consecutive block.
 */
bool CBCache::IsRoomInCache(long blockNum) const
{
    if (fFirstBlock == kEmpty)
        return true;

    // already in cache?
    if (blockNum >= fFirstBlock && blockNum < fFirstBlock + fNumBlocks)
        return true;

    // running off the end?
    if (fNumBlocks == kMaxCachedBlocks)
        return false;

    // is it the exact next one?
    if (fFirstBlock + fNumBlocks != blockNum)
        return false;

    return true;
}

/*
 * Add a block to the cache.
 *
 * We might be adding it after a read or a write.  The "isDirty" flag
 * tells us what the deal is.  If somebody tries to overwrite a dirty
 * block with a new one and doesn't have "isDirty" set, it probably means
 * they're trying to overwrite dirty cached data with the result of a new
 * read, which is a bug.  Trap it here.
 */
DIError CBCache::PutInCache(long blockNum, const void* buf, bool isDirty)
{
    int blockOffset = -1;
    if (!IsRoomInCache(blockNum)) {
        assert(false);
        return kDIErrInternal;
    }

    if (fFirstBlock == kEmpty) {
        //LOGI("   CBCache: starting anew with block %ld", blockNum);
        fFirstBlock = blockNum;
        fNumBlocks = 1;
        blockOffset = 0;
    } else if (blockNum == fFirstBlock + fNumBlocks) {
        //LOGI("   CBCache: appending block %ld", blockNum);
        blockOffset = fNumBlocks;
        fNumBlocks++;
    } else if (blockNum >= fFirstBlock && blockNum < fFirstBlock + fNumBlocks) {
        blockOffset = blockNum - fFirstBlock;
    } else {
        assert(false);
        return kDIErrInternal;
    }
    assert(blockOffset != -1);
    assert(blockOffset < kMaxCachedBlocks);

    if (fDirty[blockOffset] && !isDirty) {
        LOGI("BUG: CBCache trying to clear dirty flag for block %ld",
            blockNum);
        assert(false);
        return kDIErrInternal;
    }
    fDirty[blockOffset] = isDirty;

    //LOGI("   CBCache: adding block %d to cache at %d", blockNum, blockOffset);
    int offset = blockOffset * kBlockSize;
    assert(offset >= 0);

    memcpy(fCache + offset, buf, kBlockSize);
    return kDIErrNone;
}

/*
 * Determine whether there are any dirty blocks in the cache.
 */
bool CBCache::IsDirty(void) const
{
    if (fFirstBlock == kEmpty)
        return false;

    assert(fNumBlocks > 0);
    for (int i = 0; i < fNumBlocks; i++) {
        if (fDirty[i]) {
            //LOGI("   CBCache: dirty blocks found");
            return true;
        }
    }

    //LOGI("   CBCache: no dirty blocks found");
    return false;
}

/*
 * Return a pointer to the cache goodies, so that the object sitting
 * on the disk hardware can write our stuff.
 */
void CBCache::GetCachePointer(long* pFirstBlock, int* pNumBlocks, void** pBuf) const
{
    assert(fFirstBlock != kEmpty);  // not essential, but why call here if not?

    *pFirstBlock = fFirstBlock;
    *pNumBlocks = fNumBlocks;
    *pBuf = (void*) fCache;
}

/*
 * Clear all the dirty flags.
 */
void CBCache::Scrub(void)
{
    if (fFirstBlock == kEmpty)
        return;

    for (int i = 0; i < fNumBlocks; i++)
        fDirty[i] = false;
}

/*
 * Trash all of our entries.  If any are dirty, scream bloody murder.
 */
void CBCache::Purge(void)
{
    if (fFirstBlock == kEmpty)
        return;

    if (IsDirty()) {
        // Should only happen after a write failure causes us to clean up.
        LOGE("HEY: CBCache purging dirty blocks!");
        //assert(false);
    }
    Scrub();

    fFirstBlock = kEmpty;
    fNumBlocks = 0;
}

#endif /*_WIN32*/
