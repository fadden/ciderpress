/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Declarations for the Win32 SCSI Pass-Through Interface.
 */
#ifndef DISKIMG_SPTI_H
#define DISKIMG_SPTI_H

#ifdef _WIN32

namespace DiskImgLib {

/*
 * This is currently implemented as a set of static functions.  Do not
 * instantiate the class.
 */
class DISKIMG_API SPTI {
public:
    // Read blocks from the device.
    static DIError ReadBlocks(HANDLE handle, long startBlock, short numBlocks,
        long blockSize, void* buf);

    // Get the capacity, expressed as the highest-available LBA and the device
    //  block size.
    static DIError GetDeviceCapacity(HANDLE handle, uint32_t* pLastBlock,
        uint32_t* pBlockSize);

private:
    SPTI(void) {}
    ~SPTI(void) {}
};

}   // namespace DiskImgLib

#endif /*_WIN32*/

#endif /*DISKIMG_SPTI_H*/
