/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Implementation of some SPTI functions.
 */
#include "StdAfx.h"
#ifdef _WIN32

#include "DiskImgPriv.h"
#include "SCSIDefs.h"
#include "CP_ntddscsi.h"
#include "SPTI.h"


/*
 * Get the capacity of the device.
 *
 * Returns the LBA of the last valid block and the device's block size.
 */
/*static*/ DIError
SPTI::GetDeviceCapacity(HANDLE handle, unsigned long* pLastBlock,
    unsigned long* pBlockSize)
{
    SCSI_PASS_THROUGH_DIRECT sptd;
    unsigned long lba, blockLen;
    CDB_ReadCapacityData dataBuf;
    DWORD cb;
    BOOL status;

    assert(sizeof(dataBuf) == 8);   // READ CAPACITY returns two longs

    memset(&sptd, 0, sizeof(sptd));
    sptd.Length = sizeof(sptd);
    sptd.PathId = 0;            // SCSI card ID filled in by ioctl
    sptd.TargetId = 0;          // SCSI target ID filled in by ioctl
    sptd.Lun = 0;               // SCSI lun ID filled in by ioctl
    sptd.CdbLength = 10;        // CDB size is 10 for READ CAPACITY
    sptd.SenseInfoLength = 0;   // don't return any sense data
    sptd.DataIn = SCSI_IOCTL_DATA_IN;   // will be data from drive
    sptd.DataTransferLength = sizeof(dataBuf);
    sptd.TimeOutValue = 10;     // SCSI timeout value, in seconds
    sptd.DataBuffer = (PVOID) &dataBuf;
    sptd.SenseInfoOffset = 0;   // offset to request-sense buffer

    CDB10* pCdb = (CDB10*) &sptd.Cdb;
    pCdb->operationCode = kScsiOpReadCapacity;
    // rest of CDB is zero

    status = ::DeviceIoControl(handle, IOCTL_SCSI_PASS_THROUGH_DIRECT,
        &sptd, sizeof(sptd), NULL, 0, &cb, NULL);

    if (!status) {
        DWORD lastError = ::GetLastError();
        LOGI("DeviceIoControl(SCSI READ CAPACITY) failed, err=%ld",
            ::GetLastError());
        if (lastError == ERROR_IO_DEVICE)   // no disc in drive
            return kDIErrDeviceNotReady;
        else
            return kDIErrSPTIFailure;
    }

    lba =   (unsigned long) dataBuf.logicalBlockAddr0 << 24 |
            (unsigned long) dataBuf.logicalBlockAddr1 << 16 |
            (unsigned long) dataBuf.logicalBlockAddr2 << 8 |
            (unsigned long) dataBuf.logicalBlockAddr3;
    blockLen = (unsigned long) dataBuf.bytesPerBlock0 << 24 |
            (unsigned long) dataBuf.bytesPerBlock1 << 16 |
            (unsigned long) dataBuf.bytesPerBlock2 << 8 |
            (unsigned long) dataBuf.bytesPerBlock3;

    *pLastBlock = lba;
    *pBlockSize = blockLen;

    return kDIErrNone;
}


/*
 * Read one or more blocks from the specified SCSI device.
 *
 * "buf" must be able to hold (numBlocks * blockSize) bytes.
 */
/*static*/ DIError
SPTI::ReadBlocks(HANDLE handle, long startBlock, short numBlocks,
    long blockSize, void* buf)
{
    SCSI_PASS_THROUGH_DIRECT sptd;
    DWORD cb;
    BOOL status;

    assert(startBlock >= 0);
    assert(numBlocks > 0);
    assert(buf != NULL);

    //LOGI(" SPTI phys read block (%ld) %d", startBlock, numBlocks);

    memset(&sptd, 0, sizeof(sptd));
    sptd.Length = sizeof(sptd); // size of struct (+ request-sense buffer)
    sptd.ScsiStatus = 0;
    sptd.PathId = 0;            // SCSI card ID filled in by ioctl
    sptd.TargetId = 0;          // SCSI target ID filled in by ioctl
    sptd.Lun = 0;               // SCSI lun ID filled in by ioctl
    sptd.CdbLength = 10;        // CDB size is 10 for READ CAPACITY
    sptd.SenseInfoLength = 0;   // don't return any sense data
    sptd.DataIn = SCSI_IOCTL_DATA_IN;   // will be data from drive
    sptd.DataTransferLength = blockSize * numBlocks;
    sptd.TimeOutValue = 10;     // SCSI timeout value (in seconds)
    sptd.DataBuffer = (PVOID) buf;
    sptd.SenseInfoOffset = 0;   // offset from start of struct to request-sense

    CDB10* pCdb = (CDB10*) &sptd.Cdb;
    pCdb->operationCode = kScsiOpRead;
    pCdb->logicalBlockAddr0 = (unsigned char) (startBlock >> 24);   // MSB
    pCdb->logicalBlockAddr1 = (unsigned char) (startBlock >> 16);
    pCdb->logicalBlockAddr2 = (unsigned char) (startBlock >> 8);
    pCdb->logicalBlockAddr3 = (unsigned char) startBlock;           // LSB
    pCdb->transferLength0 = (unsigned char) (numBlocks >> 8);       // MSB
    pCdb->transferLength1 = (unsigned char) numBlocks;              // LSB

    status = ::DeviceIoControl(handle, IOCTL_SCSI_PASS_THROUGH_DIRECT,
        &sptd, sizeof(sptd), NULL, 0, &cb, NULL);

    if (!status) {
        LOGI("DeviceIoControl(SCSI READ(10)) failed, err=%ld",
            ::GetLastError());
        return kDIErrReadFailed;    // close enough
    }
    if (sptd.ScsiStatus != 0) {
        LOGI("SCSI READ(10) failed, status=%d", sptd.ScsiStatus);
        return kDIErrReadFailed;
    }

    return kDIErrNone;
}

#endif /*_WIN32*/
