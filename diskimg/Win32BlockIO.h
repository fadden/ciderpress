/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
#ifdef _WIN32
/*
 * Structures and functions for performing block-level I/O on Win32 logical
 * and physical volumes.
 *
 * Under Win2K/XP this is pretty straightforward: open the volume and
 * issue seek and read calls.  It's not quite that simple -- reads need to
 * be in 512-byte sectors for floppy and hard drives, seeks need to be on
 * sector boundaries, and you can't seek from the end, which makes it hard
 * to figure out how big the volume is -- but it's palatable.
 *
 * Under Win95/Win98/WinME, life is more difficult.  You need to use the
 * Int21h/7305h services to access logical volumes.  Of course, those weren't
 * available until Win95 OSR2, before which you used Int25h/6000h, but those
 * don't work with FAT32 volumes.  Access to physical devices requires Int13h,
 * which is fine for floppies but requires 16-bit flat thunks for hard drives
 * (see Q137176: "DeviceIoControl Int 13h Does Not Support Hard Disks").
 *
 * If Win98 can't recognize the volume on a floppy, it tries to reacquire
 * the volume information every time you ask it to read a sector.  This makes
 * things *VERY* slow.  The solution is to use the physical drive Int13h
 * services.  These come in two variants, one of which will work on just
 * about any machine but only works with floppies.  The other will work on
 * anything built since about 1996.
 *
 * Figuring out whether something is or isn't a floppy requires yet
 * another call.  All things considered it's quite an ordeal.  The block I/O
 * functions are wrapped up in classes so nobody else has to worry about all
 * this mess.
 *
 * Implementation note: this class is broken down by how the devices are
 * opened, e.g. logical, physical, or ASPI address.  Breaking it down by device
 * type seems more appropriate, but Win98 vs Win2K can require completely
 * different approaches (e.g. physical vs. logical for floppy disk, logical
 * vs. ASPI for CD-ROM).  There is no perfect decomposition.
 *
 * Summary:
 *  Win9x/ME physical drive: Int13h (doesn't work for hard drives)
 *  Win9x/ME logical drive: Int21h/7305h
 *  Win9x/ME SCSI drive or CD-ROM drive: ASPI
 *  Win2K/XP physical drive: CreateFile("\\.\PhysicalDriveN")
 *  Win2K/XP logical drive: CreateFile("\\.\X")
 *  Win2K/XP SCSI drive or CD-ROM drive: SPTI
 */
#ifndef DISKIMG_WIN32BLOCKIO_H
#define DISKIMG_WIN32BLOCKIO_H


namespace DiskImgLib {

extern bool IsWin9x(void);


/*
 * Cache a contiguous set of blocks.  This was originally motivated by poor
 * write performance, but that problem was largely solved in other ways.
 * It's still handy to write an entire track at once under Win98 though.
 *
 * Only storing continuous runs of blocks makes the cache less useful, but
 * much easier to write, and hence less likely to break in unpleasant ways.
 *
 * This class just manages the blocks.  The FlushCache() function in
 * Win32LogicalVolume is responsible for actually pushing the writes through.
 *
 * (I'm not entirely happy with this, especially since it doesn't take into
 * account the underlying device block size.  This could've been a good place
 * to handle the 2048-byte CD-ROM block size, rather than caching it again in
 * the CD-ROM handler.)
 */
class CBCache {
public:
    CBCache(void) : fFirstBlock(kEmpty), fNumBlocks(0)
    {
        for (int i = 0; i < kMaxCachedBlocks; i++)
            fDirty[i] = false;
    }
    virtual ~CBCache(void) { Purge(); }

    enum { kEmpty = -1 };

    // is the block we want in the cache?
    bool IsBlockInCache(long blockNum) const;
    // read block out of cache (after verifying that it's present)
    DIError GetFromCache(long blockNum, void* buf);
    // can the cache store this block?
    bool IsRoomInCache(long blockNum) const;
    // write block to cache (after verifying that it will fit)
    DIError PutInCache(long blockNum, const void* buf, bool isDirty);

    // are there any dirty blocks in the cache?
    bool IsDirty(void) const;
    // get start, count, and buffer so we can write the cached data
    void GetCachePointer(long* pFirstBlock, int* pNumBlocks, void** pBuf) const;
    // clear all the dirty flags
    void Scrub(void);
    // purge all cache entries (ideally after writing w/help from GetCachePtr)
    void Purge(void);

private:
    enum {
        kMaxCachedBlocks = 18,      // one track on 1.4MB floppy
        kBlockSize = 512,           // must match with Win32LogicalVolume::
    };

    long        fFirstBlock;        // set to kEmpty when cache is empty
    int         fNumBlocks;
    bool        fDirty[kMaxCachedBlocks];
    unsigned char   fCache[kMaxCachedBlocks * kBlockSize];
};


/*
 * This class encapsulates block access to a logical or physical volume.
 */
class Win32VolumeAccess {
public:
    Win32VolumeAccess(void) : fpBlockAccess(NULL)
    {}
    virtual ~Win32VolumeAccess(void) {
        if (fpBlockAccess != NULL) {
            FlushCache(true);
            fpBlockAccess->Close();
        }
    }

    // "deviceName" has the form "X:\" (logical), "81:\" (physical), or
    //  "ASPI:x:y:z\" (ASPI)
    DIError Open(const WCHAR* deviceName, bool readOnly);
    // close the device
    void Close(void);
    // is the device open and working?
    bool Ready(void) const { return fpBlockAccess != NULL; }

    // return the volume's EOF
    long GetTotalBlocks(void) const { return fTotalBlocks; }
    // return the block size for this volume (always a power of 2)
    int GetBlockSize(void) const { return BlockAccess::kBlockSize; }

    // read one or more consecutive blocks
    DIError ReadBlocks(long startBlock, short blockCount, void* buf);
    // write one or more consecutive blocks
    DIError WriteBlocks(long startBlock, short blockCount, const void* buf);
    // flush our internal cache
    DIError FlushCache(bool purge);

private:
    /*
     * Abstract base class with some handy functions.
     */
    class BlockAccess {
    public:
        BlockAccess(void) { fIsWin9x = DiskImgLib::IsWin9x(); }
        virtual ~BlockAccess(void) {}

        typedef struct {
            int     numCyls;
            int     numHeads;
            int     numSectors;
            long    blockCount;     // total #of blocks on this kind of disk
        } DiskGeometry;

        // generic interfaces
        virtual DIError Open(const WCHAR* deviceName, bool readOnly) = 0;
        virtual DIError DetectCapacity(long* pNumBlocks) = 0;
        virtual DIError ReadBlocks(long startBlock, short blockCount,
            void* buf) = 0;
        virtual DIError WriteBlocks(long startBlock, short blockCount,
            const void* buf) = 0;
        virtual DIError Close(void) = 0;

        static bool BlockToCylinderHeadSector(long blockNum,
            const DiskGeometry* pGeometry, int* pCylinder, int* pHead,
            int* pSector, long* pLastBlockOnTrack);

        enum {
            kNumLogicalVolumes = 26,    // A-Z
            kBlockSize = 512,
            kCDROMSectorSize = 2048,
            kMaxFloppyRetries = 3,      // retry floppy reads/writes
        };

        // BIOS floppy disk drive type; doubles here as media type
        typedef enum {
            kFloppyUnknown = 0,
            kFloppy525_360 = 1,
            kFloppy525_1200 = 2,
            kFloppy35_720 = 3,
            kFloppy35_1440 = 4,
            kFloppy35_2880 = 5,

            kFloppyMax
        } FloppyKind;

    protected:
        static DIError GetFloppyDriveKind(HANDLE handle, int unitNum,
            FloppyKind* pKind);
        // detect the #of blocks on the volume
        static DIError ScanCapacity(BlockAccess* pThis, long* pNumBlocks);
        // determine whether a block is readable
        static bool CanReadBlock(BlockAccess* pThis, long blockNum);
        // try to detect device capacity using SPTI
        DIError DetectCapacitySPTI(HANDLE handle,
            bool isCDROM, long* pNumBlocks);

        static int ReadBlocksInt13h(HANDLE handle, int unitNum,
            int cylinder, int head, int sector, short blockCount, void* buf);
        static DIError ReadBlocksInt13h(HANDLE handle, int unitNum,
            const DiskGeometry* pGeometry, long startBlock, short blockCount,
            void* buf);
        static int WriteBlocksInt13h(HANDLE handle, int unitNum,
            int cylinder, int head, int sector, short blockCount,
            const void* buf);
        static DIError WriteBlocksInt13h(HANDLE handle, int unitNum,
            const DiskGeometry* pGeometry, long startBlock, short blockCount,
            const void* buf);

        static DIError ReadBlocksInt21h(HANDLE handle, int driveNum,
            long startBlock, short blockCount, void* buf);
        static DIError WriteBlocksInt21h(HANDLE handle, int driveNum,
            long startBlock, short blockCount, const void* buf);

        static DIError ReadBlocksWin2K(HANDLE handle,
            long startBlock, short blockCount, void* buf);
        static DIError WriteBlocksWin2K(HANDLE handle,
            long startBlock, short blockCount, const void* buf);

        bool    fIsWin9x;           // Win9x/ME=true, Win2K/XP=false
    };

    /*
     * Access to a logical volume (e.g. "C:\") under Win9x and Win2K/XP.
     */
    class LogicalBlockAccess : public BlockAccess {
    public:
        LogicalBlockAccess(void) : fHandle(NULL), fIsCDROM(false),
            fDriveNum(-1), fLastSectorCache(NULL), fLastSectorNum(-1)
            {}
        virtual ~LogicalBlockAccess(void) {
            if (fHandle != NULL) {
                //LOGI("HEY: LogicalBlockAccess: forcing close");
                Close();
            }
            delete[] fLastSectorCache;
        }

        virtual DIError Open(const WCHAR* deviceName, bool readOnly);
        virtual DIError DetectCapacity(long* pNumBlocks) {
            /* use SCSI length value if at all possible */
            DIError dierr;
            dierr = DetectCapacitySPTI(fHandle, fIsCDROM, pNumBlocks);
            if (fIsCDROM)
                return dierr;       // SPTI should always work for CD-ROM
            if (dierr != kDIErrNone)
                return ScanCapacity(this, pNumBlocks);  // fall back on scan
            else
                return dierr;
        }
        virtual DIError ReadBlocks(long startBlock, short blockCount,
            void* buf)
        {
            if (fIsCDROM)
                return ReadBlocksCDROM(fHandle, startBlock, blockCount, buf);
            if (fIsWin9x)
                return ReadBlocksInt21h(fHandle, fDriveNum, startBlock,
                            blockCount, buf);
            else
                return ReadBlocksWin2K(fHandle, startBlock, blockCount, buf);
        }
        virtual DIError WriteBlocks(long startBlock, short blockCount,
            const void* buf)
        {
            if (fIsCDROM)
                return kDIErrWriteProtected;
            if (fIsWin9x)
                return WriteBlocksInt21h(fHandle, fDriveNum, startBlock,
                            blockCount, buf);
            else
                return WriteBlocksWin2K(fHandle, startBlock, blockCount, buf);
        }
        virtual DIError Close(void);

    private:
        //DIError DetectCapacitySPTI(long* pNumBlocks);
        DIError ReadBlocksCDROM(HANDLE handle,
            long startBlock, short numBlocks, void* buf);

        // Win2K/XP and Win9x/ME
        HANDLE          fHandle;
        bool            fIsCDROM;       // set for CD-ROM devices
        // Win9x/ME
        int             fDriveNum;      // 1=A, 3=C, etc
        // CD-ROM goodies
        unsigned char*  fLastSectorCache;
        long            fLastSectorNum;
    };

    /*
     * Access to a physical volume (e.g. 00h or 80h) under Win9x and
     * Win2K/XP.
     */
    class PhysicalBlockAccess : public BlockAccess {
    public:
        PhysicalBlockAccess(void) : fHandle(NULL), fInt13Unit(-1) {}
        virtual ~PhysicalBlockAccess(void) {}

        virtual DIError Open(const WCHAR* deviceName, bool readOnly);
        virtual DIError DetectCapacity(long* pNumBlocks) {
            /* try SPTI in case it happens to work */
            DIError dierr;
            dierr = DetectCapacitySPTI(fHandle, false, pNumBlocks);
            if (dierr != kDIErrNone)
                return ScanCapacity(this, pNumBlocks);
            else
                return dierr;
        }
        virtual DIError ReadBlocks(long startBlock, short blockCount,
            void* buf)
        {
            if (fIsWin9x)
                return ReadBlocksInt13h(fHandle, fInt13Unit,
                            &fGeometry, startBlock, blockCount, buf);
            else
                return ReadBlocksWin2K(fHandle,
                            startBlock, blockCount, buf);
        }
        virtual DIError WriteBlocks(long startBlock, short blockCount,
            const void* buf)
        {
            if (fIsWin9x)
                return WriteBlocksInt13h(fHandle, fInt13Unit,
                            &fGeometry, startBlock, blockCount, buf);
            else
                return WriteBlocksWin2K(fHandle,
                            startBlock, blockCount, buf);
        }
        virtual DIError Close(void);

    private:
        DIError DetectFloppyGeometry(void);

        // Win2K/XP
        HANDLE          fHandle;
        // Win9x/ME
        int             fInt13Unit;     // 00h=floppy #1, 80h=HD#1
        FloppyKind      fFloppyKind;
        DiskGeometry    fGeometry;
    };

#ifdef WANT_ASPI
    /*
     * Access to a SCSI volume via the ASPI interface.
     */
    class ASPIBlockAccess : public BlockAccess {
    public:
        ASPIBlockAccess(void) : fpASPI(NULL),
            fAdapter(0xff), fTarget(0xff), fLun(0xff), fReadOnly(false),
            fLastChunkCache(NULL), fLastChunkNum(-1), fChunkSize(-1)
            {}
        virtual ~ASPIBlockAccess(void) { delete[] fLastChunkCache; }

        virtual DIError Open(const char* deviceName, bool readOnly);
        virtual DIError DetectCapacity(long* pNumBlocks);
        virtual DIError ReadBlocks(long startBlock, short blockCount,
            void* buf);
        virtual DIError WriteBlocks(long startBlock, short blockCount,
            const void* buf);
        virtual DIError Close(void);

    private:
        int ExtractInt(const char** pStr, int* pResult);

        ASPI*           fpASPI;
        unsigned char   fAdapter;
        unsigned char   fTarget;
        unsigned char   fLun;

        bool            fReadOnly;

        // block cache
        unsigned char*  fLastChunkCache;
        long            fLastChunkNum;
        long            fChunkSize;     // set by DetectCapacity
    };
#endif /*WANT_ASPI*/

    // write a series of blocks to the volume
    DIError DoWriteBlocks(long startBlock, short blockCount, const void* buf)
    {
        return fpBlockAccess->WriteBlocks(startBlock, blockCount, buf);
    }

    long            fTotalBlocks;
    BlockAccess*    fpBlockAccess;  // really LogicalBA or PhysicalBA
    CBCache         fBlockCache;
};


}; // namespace DiskImgLib

#endif /*DISKIMG_WIN32BLOCKIO_H*/

#endif /*_WIN32*/
