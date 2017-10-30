/*
 * CiderPress
 * Copyright (C) 2009 by CiderPress authors.  All Rights Reserved.
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Public declarations for the DiskImg library.
 *
 * Everything is wrapped in the "DiskImgLib" namespace.  Either prefix
 * all references with "DiskImgLib::", or add "using namespace DiskImgLib"
 * to all C++ source files that make use of it.
 *
 * Under Linux, this should be compiled with -D_FILE_OFFSET_BITS=64.
 *
 * These classes are not thread-safe with respect to access to a single
 * disk image.  Writing to the same disk image from multiple threads
 * simultaneously is bound to end in disaster.  Simultaneous access to
 * different objects will work, though modifying the same disk image
 * file from multiple objects will lead to unpredictable results.
 */
#ifndef DISKIMG_DISKIMG_H
#define DISKIMG_DISKIMG_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>

//#define EXCISE_GPL_CODE

/* Windows DLL stuff */
#ifdef _WIN32
# ifdef DISKIMG_EXPORTS
#  define DISKIMG_API __declspec(dllexport)
# else
#  define DISKIMG_API __declspec(dllimport)
# endif
#else
# define DISKIMG_API
#endif

namespace DiskImgLib {

/* compiled-against versions; call DiskImg::GetVersion for linked-against */
#define kDiskImgVersionMajor    5
#define kDiskImgVersionMinor    0
#define kDiskImgVersionBug      1


/*
 * Errors from the various disk image classes.
 */
typedef enum DIError {
    kDIErrNone                  = 0,

    /* I/O request errors (should renumber/rename to match GS/OS errors?) */
    kDIErrAccessDenied          = -10,
    kDIErrVWAccessForbidden     = -11,  // write access to volume forbidden
    kDIErrSharingViolation      = -12,  // file is in use and not shareable
    kDIErrNoExclusiveAccess     = -13,  // couldn't get exclusive access
    kDIErrWriteProtected        = -14,  // disk is write protected
    kDIErrCDROMNotSupported     = -15,  // access to CD-ROM drives not supptd
    kDIErrASPIFailure           = -16,  // generic ASPI failure result
    kDIErrSPTIFailure           = -17,  // generic SPTI failure result
    kDIErrSCSIFailure           = -18,  // generic SCSI failure result
    kDIErrDeviceNotReady        = -19,  // floppy or CD-ROM drive has no media

    kDIErrFileNotFound          = -20,
    kDIErrForkNotFound          = -21,  // requested fork does not exist
    kDIErrAlreadyOpen           = -22,  // already open, can't open a 2nd time
    kDIErrFileOpen              = -23,  // file is open, can't delete it
    kDIErrNotReady              = -24,
    kDIErrFileExists            = -25,  // file already exists
    kDIErrDirectoryExists       = -26,  // directory already exists

    kDIErrEOF                   = -30,  // end-of-file reached
    kDIErrReadFailed            = -31,
    kDIErrWriteFailed           = -32,
    kDIErrDataUnderrun          = -33,  // tried to read off end of the image
    kDIErrDataOverrun           = -34,  // tried to write off end of the image
    kDIErrGenericIO             = -35,  // generic I/O error

    kDIErrOddLength             = -40,  // image size not multiple of sectors
    kDIErrUnrecognizedFileFmt   = -41,  // file format just not recognized
    kDIErrBadFileFormat         = -42,  // filename ext doesn't match contents
    kDIErrUnsupportedFileFmt    = -43,  // recognized but not supported
    kDIErrUnsupportedPhysicalFmt = -44, // (same)
    kDIErrUnsupportedFSFmt      = -45,  // (and again)
    kDIErrBadOrdering           = -46,  // requested sector ordering no good
    kDIErrFilesystemNotFound    = -47,  // requested filesystem isn't there
    kDIErrUnsupportedAccess     = -48,  // e.g. read sectors from blocks-only
    kDIErrUnsupportedImageFeature = -49, // e.g. FDI image w/Amiga sectors

    kDIErrInvalidTrack          = -50,  // request for invalid track number
    kDIErrInvalidSector         = -51,  // request for invalid sector number
    kDIErrInvalidBlock          = -52,  // request for invalid block number
    kDIErrInvalidIndex          = -53,  // request with an invalid index

    kDIErrDirectoryLoop         = -60,  // directory chain points into itself
    kDIErrFileLoop              = -61,  // file sector or block alloc loops
    kDIErrBadDiskImage          = -62,  // the FS on the disk image is damaged
    kDIErrBadFile               = -63,  // bad file on disk image
    kDIErrBadDirectory          = -64,  // bad dir on disk image
    kDIErrBadPartition          = -65,  // bad partition on multi-part format

    kDIErrFileArchive           = -70,  // file archive, not disk archive
    kDIErrUnsupportedCompression = -71, // compression method is not supported
    kDIErrBadChecksum           = -72,  // image file's checksum is bad
    kDIErrBadCompressedData     = -73,  // data can't even be unpacked
    kDIErrBadArchiveStruct      = -74,  // bad archive structure

    kDIErrBadNibbleSectors      = -80,  // can't read sectors from this image
    kDIErrSectorUnreadable      = -81,  // requested sector not readable
    kDIErrInvalidDiskByte       = -82,  // invalid byte for encoding type
    kDIErrBadRawData            = -83,  // couldn't get correct nibbles

    kDIErrInvalidFileName       = -90,  // tried to create file with bad name
    kDIErrDiskFull              = -91,  // no space left on disk
    kDIErrVolumeDirFull         = -92,  // no more entries in volume dir
    kDIErrInvalidCreateReq      = -93,  // CreateImage request was flawed
    kDIErrTooBig                = -94,  // larger than we want to handle

    /* higher-level errors */
    kDIErrGeneric               = -101,
    kDIErrInternal              = -102,
    kDIErrMalloc                = -103,
    kDIErrInvalidArg            = -104,
    kDIErrNotSupported          = -105, // feature not currently supported
    kDIErrCancelled             = -106, // an operation was cancelled by user

    kDIErrNufxLibInitFailed     = -110,
} DIError;

/* return a string describing the error */
DISKIMG_API const char* DIStrError(DIError dierr);


/* exact definition of off_t varies, so just define our own */
#ifdef _ULONGLONG_
 typedef LONGLONG di_off_t;
#else
 typedef off_t di_off_t;
#endif

/* common definition of "whence" for seeks */
enum DIWhence {
    kSeekSet = SEEK_SET,
    kSeekCur = SEEK_CUR,
    kSeekEnd = SEEK_END
};

/* try to load ASPI under Win2K; if successful, SPTI should be disabled */
const bool kAlwaysTryASPI = false;
/* ASPI device "filenames" look like "ASPI:x:y:z\" */
DISKIMG_API extern const char* kASPIDev;

/* some nibble-encoding constants */
const int kTrackLenNib525 = 6656;
const int kTrackLenNb2525 = 6384;
const int kTrackLenTrackStar525 = 6525; // max len of data in TS image
const int kTrackAllocSize = 6656;   // max 5.25 nibble track len; for buffers
const int kTrackCount525 = 35;      // expected #of tracks on 5.25 img
const int kMaxNibbleTracks525 = 40; // max #of tracks on 5.25 nibble img
const int kDefaultNibbleVolumeNum = 254;
const int kBlockSize = 512;         // block size for DiskImg interfaces
const int kSectorSize = 256;        // sector size (1/2 block)
const int kD13Length = 256 * 13 * 35;   // length of a .d13 image

/* largest expanse we allow access to on a volume (8GB in 512-byte blocks) */
const long kVolumeMaxBlocks = 8*1024*(1024*1024 / kBlockSize);

/* largest .gz file we'll open (uncompressed size) */
const long kGzipMax = 32*1024*1024;

/* forward and external class definitions */
class DiskFS;
class A2File;
class A2FileDescr;
class GenericFD;
class OuterWrapper;
class ImageWrapper;
class CircularBufferAccess;
class ASPI;
class LinearBitmap;


/*
 * Library-global data functions.
 *
 * This class is just a namespace clumper.  Do not instantiate.
 */
class DISKIMG_API Global {
public:
    // one-time DLL initialization; use SetDebugMsgHandler first
    static DIError AppInit(void);
    // one-time DLL cleanup
    static DIError AppCleanup(void);

    // return the DiskImg version number
    static void GetVersion(int32_t* pMajor, int32_t* pMinor, int32_t* pBug);

    static bool GetAppInitCalled(void) { return fAppInitCalled; }
    static bool GetHasSPTI(void);
    static bool GetHasASPI(void);

    // return a pointer to our global ASPI instance, or NULL
    static ASPI* GetASPI(void) { return fpASPI; }
    // shortcut for fpASPI->GetVersion()
    static unsigned long GetASPIVersion(void);

    // pointer to the debug message handler
    typedef void (*DebugMsgHandler)(const char* file, int line, const char* msg);
    static DebugMsgHandler gDebugMsgHandler;

    static DebugMsgHandler SetDebugMsgHandler(DebugMsgHandler handler);
    static void PrintDebugMsg(const char* file, int line, const char* fmt, ...)
        #if defined(__GNUC__)
            __attribute__ ((format(printf, 3, 4)))
        #endif
        ;

private:
    // no instantiation allowed
    Global(void) {}
    ~Global(void) {}

    // make sure app calls AppInit
    static bool fAppInitCalled;

    static ASPI*    fpASPI;
};

extern bool gAllowWritePhys0;   // ugh -- see Win32BlockIO.cpp


/*
 * Disk I/O class, roughly equivalent to a GS/OS disk device driver.
 *
 * Abstracts away the file's source (file on disk, file in memory) and
 * storage format (DOS order, ProDOS order, nibble).  Will also cope
 * with common disk compression and wrapper formats (Mac DiskCopy, 2MG,
 * ShrinkIt, etc) if handed a file on disk.
 *
 * Images may be embedded within other images, e.g. UNIDOS and storage
 * type $04 pascal volumes.
 *
 * THOUGHT: we need a list(?) of pointers from here back to the DiskFS
 * so that any modifications here will "wake" the DiskFS and sub-volumes.
 * We also need a "dirty" flag so things like CloseNufx can know not to
 * re-do work when Closing after a Flush.  Also DiskFS can alert us to
 * any locally cached stuff, and we can tell them to flush everything.
 * (Possibly useful when doing disk updates, so stuff can be trivially
 * un-done.  Currently CiderPress checks the filename manually after
 * each write, but that's generally less reliable than having the knowledge
 * contained in the DiskImg.)
 *
 * THOUGHT: need a ReadRawTrack that gets raw nibblized data.  For a
 * nibblized image it returns the data, for a sector image it generates
 * the raw data.
 *
 * THOUGHT: we could reduce the risk of problems and increase performance
 * for physical media with a "copy on write" scheme.  We'd create a shadow
 * array of modified blocks, and write them at Flush time.  This would
 * provide an instantaneous "revert" feature, and prevent formats like
 * DiskCopy42 (which has a CRC in its header) from being inconsistent for
 * long stretches.
 */
class DISKIMG_API DiskImg {
public:
    // create DiskImg object
    DiskImg(void);
    virtual ~DiskImg(void);

    /*
     * Types describing an image file.
     *
     * The file itself is described by an external parameter ("file source")
     * that is either the name of the file, a memory buffer, or an EFD
     * (EmbeddedFileDescriptor).
     */
    typedef enum {              // format of the "wrapper wrapper"
        kOuterFormatUnknown = 0,
        kOuterFormatNone = 1,       // (plain)
        kOuterFormatCompress = 2,   // .xx.Z
        kOuterFormatGzip = 3,       // .xx.gz
        kOuterFormatBzip2 = 4,      // .xx.bz2
        kOuterFormatZip = 10,       // .zip
    } OuterFormat;
    typedef enum {              // format of the image "wrapper"
        kFileFormatUnknown = 0,
        kFileFormatUnadorned = 1,   // .po, .do, ,nib, .raw, .d13
        kFileFormat2MG = 2,         // .2mg, .2img, $e0/0130
        kFileFormatDiskCopy42 = 3,  // .dsk/.disk, maybe .dc
        kFileFormatDiskCopy60 = 4,  // .dc6 (often just raw format)
        kFileFormatDavex = 5,       // $e0/8004
        kFileFormatSim2eHDV = 6,    // .hdv
        kFileFormatTrackStar = 7,   // .app (40-track or 80-track)
        kFileFormatFDI = 8,         // .fdi (5.25" or 3.5")
        kFileFormatNuFX = 20,       // .shk, .sdk, .bxy
        kFileFormatDDD = 21,        // .ddd
        kFileFormatDDDDeluxe = 22,  // $DD, .ddd
    } FileFormat;
    typedef enum {              // format of the image data stream
        kPhysicalFormatUnknown = 0,
        kPhysicalFormatSectors = 1, // sequential 256-byte sectors (13/16/32)
        kPhysicalFormatNib525_6656 = 2, // 5.25" disk ".nib" (6656 bytes/track)
        kPhysicalFormatNib525_6384 = 3, // 5.25" disk ".nb2" (6384 bytes/track)
        kPhysicalFormatNib525_Var = 4,  // 5.25" disk (variable len, e.g. ".app")
    } PhysicalFormat;
    typedef enum {              // sector ordering for "sector" format images
        kSectorOrderUnknown = 0,
        kSectorOrderProDOS = 1,     // written as series of ProDOS blocks
        kSectorOrderDOS = 2,        // written as series of DOS sectors
        kSectorOrderCPM = 3,        // written as series of 1K CP/M blocks
        kSectorOrderPhysical = 4,   // written as un-interleaved sectors
        kSectorOrderMax,            // (used for array sizing)
    } SectorOrder;
    typedef enum {              // main filesystem format (based on NuFX enum)
        kFormatUnknown = 0,
        kFormatProDOS = 1,
        kFormatDOS33 = 2,
        kFormatDOS32 = 3,
        kFormatPascal = 4,
        kFormatMacHFS = 5,
        kFormatMacMFS = 6,
        kFormatLisa = 7,
        kFormatCPM = 8,
        //kFormatCharFST
        kFormatMSDOS = 10,          // any FAT filesystem
        //kFormatHighSierra
        kFormatISO9660 = 12,
        //kFormatAppleShare
        kFormatRDOS33 = 20,         // 16-sector RDOS disk
        kFormatRDOS32 = 21,         // 13-sector RDOS disk
        kFormatRDOS3 = 22,          // 13-sector RDOS disk converted to 16
        // "generic" formats *must* be in their own "decade"
        kFormatGenericPhysicalOrd = 30, // unknown, but physical-sector-ordered
        kFormatGenericProDOSOrd = 31,   // unknown, but ProDOS-block-ordered
        kFormatGenericDOSOrd = 32,      // unknown, but DOS-sector-ordered
        kFormatGenericCPMOrd = 33,      // unknown, but CP/M-block-ordered
        kFormatUNIDOS = 40,         // two 400K DOS 3.3 volumes
        kFormatOzDOS = 41,          // two 400K DOS 3.3 volumes, weird order
        kFormatCFFA4 = 42,          // CFFA image with 4 or 6 partitions
        kFormatCFFA8 = 43,          // CFFA image with 8 partitions
        kFormatMacPart = 44,        // Macintosh-style partitioned disk
        kFormatMicroDrive = 45,     // ///SHH Systeme's MicroDrive format
        kFormatFocusDrive = 46,     // Parsons Engineering FocusDrive format
        kFormatGutenberg = 47,      // Gutenberg word processor format

        // try to keep this in an unsigned char, e.g. for CP clipboard
    } FSFormat;

    /*
     * Nibble encode/decode description.  Use no pointers here, so we
     * store as an array and resize at will.
     *
     * Should we define an enum to describe whether address and data
     * headers are standard or some wacky variant?
     */
    enum {
        kNibbleAddrPrologLen = 3,       // d5 aa 96
        kNibbleAddrEpilogLen = 3,       // de aa eb
        kNibbleDataPrologLen = 3,       // d5 aa ad
        kNibbleDataEpilogLen = 3,       // de aa eb
    };
    typedef enum {
        kNibbleEncUnknown = 0,
        kNibbleEnc44,
        kNibbleEnc53,
        kNibbleEnc62,
    } NibbleEnc;
    typedef enum {
        kNibbleSpecialNone = 0,
        kNibbleSpecialMuse,         // doubled sector numbers on tracks > 2
        kNibbleSpecialSkipFirstAddrByte,
    } NibbleSpecial;
    typedef struct {
        char            description[32];
        short           numSectors;     // 13 or 16 (or 18?)

        uint8_t         addrProlog[kNibbleAddrPrologLen];
        uint8_t         addrEpilog[kNibbleAddrEpilogLen];
        uint8_t         addrChecksumSeed;
        bool            addrVerifyChecksum;
        bool            addrVerifyTrack;
        int             addrEpilogVerifyCount;

        uint8_t         dataProlog[kNibbleDataPrologLen];
        uint8_t         dataEpilog[kNibbleDataEpilogLen];
        uint8_t         dataChecksumSeed;
        bool            dataVerifyChecksum;
        int             dataEpilogVerifyCount;

        NibbleEnc       encoding;
        NibbleSpecial   special;
    } NibbleDescr;


    static inline bool IsSectorFormat(PhysicalFormat fmt) {
        return (fmt == kPhysicalFormatSectors);
    }
    static inline bool IsNibbleFormat(PhysicalFormat fmt) {
        return (fmt == kPhysicalFormatNib525_6656 ||
                fmt == kPhysicalFormatNib525_6384 ||
                fmt == kPhysicalFormatNib525_Var);
    }

    // file is on disk; stuff like 2MG headers will be identified and stripped
    DIError OpenImage(const char* filename, char fssep, bool readOnly);
    // file is in memory; provide a pointer to the data start and buffer size
    DIError OpenImageFromBufferRO(const uint8_t* buffer, long length);
    // file is in memory; provide a pointer to the data start and buffer size
    DIError OpenImageFromBufferRW(uint8_t* buffer, long length);
    // file is a range of blocks on an open block-oriented disk image
    DIError OpenImage(DiskImg* pParent, long firstBlock, long numBlocks);
    // file is a range of tracks/sectors on an open sector-oriented disk image
    DIError OpenImage(DiskImg* pParent, long firstTrack, long firstSector,
                long numSectors);

    // create a new, blank image file
    DIError CreateImage(const char* pathName, const char* storageName,
                OuterFormat outerFormat, FileFormat fileFormat,
                PhysicalFormat physical, const NibbleDescr* pNibbleDescr,
                SectorOrder order, FSFormat format,
                long numBlocks, bool skipFormat);
    DIError CreateImage(const char* pathName, const char* storageName,
                OuterFormat outerFormat, FileFormat fileFormat,
                PhysicalFormat physical, const NibbleDescr* pNibbleDescr,
                SectorOrder order, FSFormat format,
                long numTracks, long numSectPerTrack, bool skipFormat);

    // flush any changes to disk; slow recompress only for "kFlushAll"
    typedef enum { kFlushUnknown=0, kFlushFastOnly=1, kFlushAll=2 } FlushMode;
    DIError FlushImage(FlushMode mode);
    // close the image, freeing up any resources in use
    DIError CloseImage(void);
    // raise/lower refCnt (may want to track pointers someday)
    void AddDiskFS(DiskFS* pDiskFS) { fDiskFSRefCnt++; }
    void RemoveDiskFS(DiskFS* pDiskFS) {
        assert(fDiskFSRefCnt > 0);
        fDiskFSRefCnt--;
    }

    // (re-)format this image in the specified FS format
    DIError FormatImage(FSFormat format, const char* volName);
    // reset all blocks/sectors to zeroes
    DIError ZeroImage(void);

    // configure for paired sectors (OzDOS)
    void SetPairedSectors(bool enable, int idx);

    // identify sector ordering and disk format
    // (may want a version that takes "hints" for special disks?)
    DIError AnalyzeImage(void);
    // figure out what FS and sector ordering is on the disk image
    void AnalyzeImageFS(void);
    bool ShowAsBlocks(void) const;
    // overrule the analyzer (generally not recommended) -- does not
    //  override FileFormat, which is very reliable
    DIError OverrideFormat(PhysicalFormat physical, FSFormat format,
        SectorOrder order);

    // Create a DiskFS that matches this DiskImg.  Must be called after
    //  AnalayzeImage, or you will always get a DiskFSUnknown.  The DiskFS
    //  must be freed with "delete" when no longer needed.
    DiskFS* OpenAppropriateDiskFS(bool allowUnknown = false);

    // Add information or a warning to the list of notes.  Use linefeeds to
    //  indicate line breaks.  This is currently append-only.
    typedef enum { kNoteInfo, kNoteWarning } NoteType;
    void AddNote(NoteType type, const char* fmt, ...)
        #if defined(__GNUC__)
            __attribute__ ((format(printf, 3, 4)))
        #endif
        ;
    const char* GetNotes(void) const;

    // simple accessors
    OuterFormat GetOuterFormat(void) const { return fOuterFormat; }
    FileFormat GetFileFormat(void) const { return fFileFormat; }
    PhysicalFormat GetPhysicalFormat(void) const { return fPhysical; }
    SectorOrder GetSectorOrder(void) const { return fOrder; }
    FSFormat GetFSFormat(void) const { return fFormat; }
    long GetNumTracks(void) const { return fNumTracks; }
    int GetNumSectPerTrack(void) const { return fNumSectPerTrack; }
    long GetNumBlocks(void) const { return fNumBlocks; }
    bool GetReadOnly(void) const { return fReadOnly; }
    bool GetDirtyFlag(void) const { return fDirty; }

    // set read-only flag; don't use this (open with correct setting;
    //  this was added as safety hack for the volume copier)
    void SetReadOnly(bool val) { fReadOnly = val; }

    // read a 256-byte sector
    // NOTE to self: this function should not be available for odd-sized
    // volumes, e.g. a ProDOS /RAM or /RAM5 stored with Davex.  Need some way
    // to communicate that to disk editor so it knows to grey-out the
    // selection checkbox and/or not use "show as sectors" as default.
    virtual DIError ReadTrackSector(long track, int sector, void* buf) {
        return ReadTrackSectorSwapped(track, sector, buf, fOrder,
                    fFileSysOrder);
    }
    DIError ReadTrackSectorSwapped(long track, int sector,
        void* buf, SectorOrder imageOrder, SectorOrder fsOrder);
    // write a 256-byte sector
    virtual DIError WriteTrackSector(long track, int sector, const void* buf);

    // read a 512-byte block
    virtual DIError ReadBlock(long block, void* buf) {
        return ReadBlockSwapped(block, buf, fOrder, fFileSysOrder);
    }
    DIError ReadBlockSwapped(long block, void* buf, SectorOrder imageOrder,
                SectorOrder fsOrder);
    // read multiple blocks
    virtual DIError ReadBlocks(long startBlock, int numBlocks, void* buf);
    // check our virtual bad block map
    bool CheckForBadBlocks(long startBlock, int numBlocks);
    // write a 512-byte block
    virtual DIError WriteBlock(long block, const void* buf);
    // write multiple blocks
    virtual DIError WriteBlocks(long startBlock, int numBlocks, const void* buf);

    // read an entire nibblized track
    virtual DIError ReadNibbleTrack(long track, uint8_t* buf,
        long* pTrackLen);
    // write a track; trackLen must be <= those in image
    virtual DIError WriteNibbleTrack(long track, const uint8_t* buf,
        long trackLen);

    // save the current image as a 2MG file
    //DIError Write2MG(const char* filename);

    // need to treat the DOS volume number as meta-data for some disks
    short GetDOSVolumeNum(void) const { return fDOSVolumeNum; }
    void SetDOSVolumeNum(short val) { fDOSVolumeNum = val; }
    enum { kVolumeNumNotSet = -1 };

    // some simple getters
    bool GetHasSectors(void) const { return fHasSectors; }
    bool GetHasBlocks(void) const { return fHasBlocks; }
    bool GetHasNibbles(void) const { return fHasNibbles; }
    bool GetIsEmbedded(void) const { return fpParentImg != NULL; }

    // return the current NibbleDescr
    const NibbleDescr* GetNibbleDescr(void) const { return fpNibbleDescr; }
    // set the NibbleDescr; we do this by copying the entry into our table
    // (could improve by doing memcmp on available entries?)
    void SetNibbleDescr(int idx);
    void SetCustomNibbleDescr(const NibbleDescr* pDescr);
    const NibbleDescr* GetNibbleDescrTable(int* pCount) const {
        *pCount = fNumNibbleDescrEntries;
        return fpNibbleDescrTable;
    }

    // set the NuFX compression type, used when compressing or re-compressing;
    // must be set before image is opened or created
    void SetNuFXCompressionType(int val) { fNuFXCompressType = val; }

    /*
     * Set up a progress callback to use when scanning a disk volume.  Pass
     * NULL for "func" to disable.
     *
     * The callback function is expected to return "true" if all is well.
     * If it returns false, kDIErrCancelled will eventually come back.
     */
    typedef bool (*ScanProgressCallback)(void* cookie, const char* str,
        int count);
    void SetScanProgressCallback(ScanProgressCallback func, void* cookie);
    /* update status dialog during disk scan; called from DiskFS code */
    bool UpdateScanProgress(const char* newStr);

    /*
     * Static utility functions.
     */
    // returns "true" if the files on this image have DOS structure, i.e.
    //  simple file types and high-ASCII text files
    static bool UsesDOSFileStructure(FSFormat format) {
        return (format == kFormatDOS33 ||
                format == kFormatDOS32 ||
                format == kFormatGutenberg ||
                format == kFormatUNIDOS ||
                format == kFormatOzDOS ||
                format == kFormatRDOS33 ||
                format == kFormatRDOS32 ||
                format == kFormatRDOS3);
    }
    // returns "true" if we can open files on the specified filesystem
    static bool CanOpenFiles(FSFormat format) {
        return (format == kFormatProDOS ||
                format == kFormatDOS33 ||
                format == kFormatDOS32 ||
                format == kFormatPascal ||
                format == kFormatCPM ||
                format == kFormatRDOS33 ||
                format == kFormatRDOS32 ||
                format == kFormatRDOS3);
    }
    // returns "true" if we can create subdirectories on this filesystem
    static bool IsHierarchical(FSFormat format) {
        return (format == kFormatProDOS ||
                format == kFormatMacHFS ||
                format == kFormatMSDOS);
    }
    // returns "true" if we can create resource forks on this filesystem
    static bool HasResourceForks(FSFormat format) {
        return (format == kFormatProDOS ||
                format == kFormatMacHFS);
    }
    // returns "true" if the format is one of the "generics"
    static bool IsGenericFormat(FSFormat format) {
        return (format / 10 == DiskImg::kFormatGenericDOSOrd / 10);
    }

    /* this must match DiskImg::kStdNibbleDescrs table */
    enum StdNibbleDescr {
        kNibbleDescrDOS33Std = 0,
        kNibbleDescrDOS33Patched,
        kNibbleDescrDOS33IgnoreChecksum,
        kNibbleDescrDOS32Std,
        kNibbleDescrDOS32Patched,
        kNibbleDescrMuse32,
        kNibbleDescrRDOS33,
        kNibbleDescrRDOS32,
        kNibbleDescrCustom,         // slot for custom entry

        kNibbleDescrMAX             // must be last
    };
    static const NibbleDescr* GetStdNibbleDescr(StdNibbleDescr idx);
    // call this once, at DLL initialization time
    static void CalcNibbleInvTables(void);
    // calculate block number from cyl/head/sect on 3.5" disk
    static int CylHeadSect35ToBlock(int cyl, int head, int sect);
    // unpack nibble data from a 3.5" disk track
    static DIError UnpackNibbleTrack35(const uint8_t* nibbleBuf,
        long nibbleLen, uint8_t* outputBuf, int cyl, int head,
        LinearBitmap* pBadBlockMap);
    // compute the #of sectors per track for cylinder N (0-79)
    static int SectorsPerTrack35(int cylinder);

    // get the order in which we test for sector ordering
    static void GetSectorOrderArray(SectorOrder* orderArray, SectorOrder first);

    // utility function used by HFS filename normalizer; available to apps
    static inline uint8_t MacToASCII(uint8_t uch) {
        if (uch < 0x20)
            return '?';
        else if (uch < 0x80)
            return uch;
        else
            return kMacHighASCII[uch - 0x80];
    }

    // Allow write access to physical disk 0.  This is usually the boot disk,
    // but with some BIOS the first IDE drive is always physical 0 even if
    // you're booting from SATA.  This only has meaning under Win32.
    static void SetAllowWritePhys0(bool val);

    /*
     * Get string constants for enumerated values.
     */
    typedef struct { int format; const char* str; } ToStringLookup;
    static const char* ToStringCommon(int format, const ToStringLookup* pTable,
        int tableSize);
    static const char* ToString(OuterFormat format);
    static const char* ToString(FileFormat format);
    static const char* ToString(PhysicalFormat format);
    static const char* ToString(SectorOrder format);
    static const char* ToString(FSFormat format);

private:
    /*
     * Fundamental disk image identification.
     */
    OuterFormat     fOuterFormat;   // e.g. gzip
    FileFormat      fFileFormat;
    PhysicalFormat  fPhysical;
    const NibbleDescr* fpNibbleDescr;  // only used for "nibble" images
    SectorOrder     fOrder;         // only used for "sector" images
    FSFormat        fFormat;

    /*
     * This affects how the DiskImg responds to requests for reading
     * a track or sector.
     *
     * "fFileSysOrder", together with with "fOrder", determines how
     * sector numbers are translated.  It describes the order that the
     * DiskFS filesystem expects things to be in.  If the image isn't
     * sector-addressable, then it is assumed to be in linear block order.
     *
     * If "fSectorPairing" is set, the DiskImg treats the disk as if
     * it were in OzDOS format, with one sector from two different
     * volumes in a single 512-byte block.
     */
    SectorOrder     fFileSysOrder;
    bool            fSectorPairing;
    int             fSectorPairOffset;  // which image (should be 0 or 1)

    /*
     * Internal state.
     */
    GenericFD*      fpOuterGFD;     // outer wrapper, if any (.gz only)
    GenericFD*      fpWrapperGFD;   // Apple II image file
    GenericFD*      fpDataGFD;      // raw Apple II data
    OuterWrapper*   fpOuterWrapper; // needed for outer .gz wrapper
    ImageWrapper*   fpImageWrapper; // disk image wrapper (2MG, SHK, etc)
    DiskImg*        fpParentImg;    // set for embedded volumes
    short           fDOSVolumeNum;  // specified by some wrapper formats
    di_off_t        fOuterLength;   // total len of file
    di_off_t        fWrappedLength; // len of file after Outer wrapper removed
    di_off_t        fLength;        // len of disk image (w/o wrappers)
    bool            fExpandable;    // ProDOS .hdv can expand
    bool            fReadOnly;      // allow writes to this image?
    bool            fDirty;         // have we modified this image?
    //bool          fIsEmbedded;    // is this image embedded in another?

    bool            fHasSectors;    // image is sector-addressable
    bool            fHasBlocks;     // image is block-addressable
    bool            fHasNibbles;    // image is nibble-addressable

    long            fNumTracks;     // for sector-addressable images
    int             fNumSectPerTrack;   // (ditto)
    long            fNumBlocks;     // for 512-byte block-addressable images

    uint8_t*        fNibbleTrackBuf;    // allocated on heap
    int             fNibbleTrackLoaded; // track currently in buffer

    int             fNuFXCompressType;  // used when compressing a NuFX image

    char*           fNotes;         // warnings and FYIs about DiskImg/DiskFS

    LinearBitmap*   fpBadBlockMap;  // used for 3.5" nibble images

    int             fDiskFSRefCnt;  // #of DiskFS objects pointing at us

    /*
     * NibbleDescr entries.  There are several standard ones, and we want
     * to allow applications to define additional ones.
     */
    NibbleDescr*    fpNibbleDescrTable;
    int             fNumNibbleDescrEntries;

    /* static table of default values */
    static const NibbleDescr kStdNibbleDescrs[];

    DIError OpenImageFromBuffer(uint8_t* buffer, long length, bool readOnly);
    DIError CreateImageCommon(const char* pathName, const char* storageName,
        bool skipFormat);
    DIError ValidateCreateFormat(void) const;
    DIError FormatSectors(GenericFD* pGFD, bool quickFormat) const;
    //DIError FormatBlocks(GenericFD* pGFD) const;

    DIError CopyBytesOut(void* buf, di_off_t offset, int size) const;
    DIError CopyBytesIn(const void* buf, di_off_t offset, int size);
    DIError AnalyzeImageFile(const char* pathName, char fssep);
    // Figure out the sector ordering for this filesystem, so we can decide
    //  how the sectors need to be re-arranged when we're reading them.
    SectorOrder CalcFSSectorOrder(void) const;
    // Handle sector order calculations.
    DIError CalcSectorAndOffset(long track, int sector, SectorOrder ImageOrder,
        SectorOrder fsOrder, di_off_t* pOffset, int* pNewSector);
    inline bool IsLinearBlocks(SectorOrder imageOrder, SectorOrder fsOrder);

    /*
     * Progress update during the filesystem scan.  This only exists in the
     * topmost DiskImg.  (This is arguably more appropriate in DiskFS, but
     * DiskFS objects don't have a notion of "parent" and are somewhat more
     * ephemeral.)
     */
    ScanProgressCallback    fpScanProgressCallback;
    void*       fScanProgressCookie;
    int         fScanCount;
    char        fScanMsg[128];
    time_t      fScanLastMsgWhen;

   /*
     * 5.25" nibble image access.
     */
    enum {
        kDataSize62 = 343,      // 342 bytes + checksum byte
        kChunkSize62 = 86,      // (0x56)

        kDataSize53 = 411,      // 410 bytes + checksum byte
        kChunkSize53 = 51,      // (0x33)
        kThreeSize = (kChunkSize53 * 3) + 1,    // same as 410 - 256
    };
    DIError ReadNibbleSector(long track, int sector, void* buf,
        const NibbleDescr* pNibbleDescr);
    DIError WriteNibbleSector(long track, int sector, const void* buf,
        const NibbleDescr* pNibbleDescr);
    void DumpNibbleDescr(const NibbleDescr* pNibDescr) const;
    int GetNibbleTrackLength(long track) const;
    int GetNibbleTrackOffset(long track) const;
    int GetNibbleTrackFormatLength(void) const {
        /* return length to use when formatting for 16 sectors */
        if (fPhysical == kPhysicalFormatNib525_6656)
            return kTrackLenNib525;
        else if (fPhysical == kPhysicalFormatNib525_6384)
            return kTrackLenNb2525;
        else if (fPhysical == kPhysicalFormatNib525_Var) {
            if (fFileFormat == kFileFormatTrackStar ||
                fFileFormat == kFileFormatFDI)
            {
                return kTrackLenNb2525; // use minimum possible
            }
        }
        assert(false);
        return -1;
    }
    int GetNibbleTrackAllocLength(void) const {
        /* return length to allocate when creating an image */
        if (fPhysical == kPhysicalFormatNib525_Var &&
            (fFileFormat == kFileFormatTrackStar ||
             fFileFormat == kFileFormatFDI))
        {
            // use maximum possible
            return kTrackLenTrackStar525;
        }
        return GetNibbleTrackFormatLength();
    }
    DIError LoadNibbleTrack(long track, long* pTrackLen);
    DIError SaveNibbleTrack(void);
    int FindNibbleSectorStart(const CircularBufferAccess& buffer,
        int track, int sector, const NibbleDescr* pNibbleDescr, int* pVol);
    void DecodeAddr(const CircularBufferAccess& buffer, int offset,
        short* pVol, short* pTrack, short* pSector, short* pChksum);
    inline uint16_t ConvFrom44(uint8_t val1, uint8_t val2) {
        return ((val1 << 1) | 0x01) & val2;
    }
    DIError DecodeNibbleData(const CircularBufferAccess& buffer, int idx,
        uint8_t* sctBuf, const NibbleDescr* pNibbleDescr);
    void EncodeNibbleData(const CircularBufferAccess& buffer, int idx,
        const uint8_t* sctBuf, const NibbleDescr* pNibbleDescr) const;
    DIError DecodeNibble62(const CircularBufferAccess& buffer, int idx,
        uint8_t* sctBuf, const NibbleDescr* pNibbleDescr);
    void EncodeNibble62(const CircularBufferAccess& buffer, int idx,
        const uint8_t* sctBuf, const NibbleDescr* pNibbleDescr) const;
    DIError DecodeNibble53(const CircularBufferAccess& buffer, int idx,
        uint8_t* sctBuf, const NibbleDescr* pNibbleDescr);
    void EncodeNibble53(const CircularBufferAccess& buffer, int idx,
        const uint8_t* sctBuf, const NibbleDescr* pNibbleDescr) const;
    int TestNibbleTrack(int track, const NibbleDescr* pNibbleDescr, int* pVol);
    DIError AnalyzeNibbleData(void);
    inline uint8_t Conv44(uint16_t val, bool first) const {
        if (first)
            return (val >> 1) | 0xaa;
        else
            return val | 0xaa;
    }
    DIError FormatNibbles(GenericFD* pGFD) const;

    static const uint8_t kMacHighASCII[];

    /*
     * 3.5" nibble access
     */
    static int FindNextSector35(const CircularBufferAccess& buffer, int start,
        int cyl, int head, int* pSector);
    static bool DecodeNibbleSector35(const CircularBufferAccess& buffer,
        int start, uint8_t* sectorBuf, uint8_t* readChecksum,
        uint8_t* calcChecksum);
    static bool UnpackChecksum35(const CircularBufferAccess& buffer,
        int offset, uint8_t* checksumBuf);
    static void EncodeNibbleSector35(const uint8_t* sectorData,
        uint8_t* outBuf);

    /* static data tables */
    static uint8_t kDiskBytes53[32];
    static uint8_t kDiskBytes62[64];
    static uint8_t kInvDiskBytes53[256];
    static uint8_t kInvDiskBytes62[256];
    enum { kInvInvalidValue = 0xff };

private:    // some C++ stuff to block behavior we don't support
    DiskImg& operator=(const DiskImg&);
    DiskImg(const DiskImg&);
};



/*
 * Disk filesystem class, roughly equivalent to a GS/OS FST.  This is an
 * abstract base class.
 *
 * Static functions know how to access a DiskImg and figure out what kind
 * of image we have.  Once known, the appropriate sub-class can be
 * instantiated.
 *
 * We maintain a linear list of files to make it easy for applications to
 * traverse the full set of files.  Sometimes this makes it hard for us to
 * update internally (especially HFS).  With some minor cleverness it
 * should be possible to switch to a tree structure while retaining the
 * linear "get next" API.  This would be a big help for ProDOS and HFS.
 *
 * NEED: some notification mechanism for changes to files and/or block
 * editing of the disk (especially with regard to open sub-volumes).  If
 * a disk volume open for file-by-file viewing is modified with the disk
 * editor, we should close the file when the disk editor exits.
 *
 * NEED? disk utilities, such as "disk crunch", for Pascal volumes.  Could
 * be written externally, but might as well keep fs knowledge embedded.
 *
 * MISSING: there is no way to override the image analyzer when working
 * with sub-volumes.  Actually, there is; it just has to happen *after*
 * the DiskFS has been created.  We should provide an approach that either
 * stifles the DiskFS creation, or allows us to override and replace the
 * internal DiskFS so we can pop up a sub-volume list, show what we *think*
 * is there, and then let the user pick a volume and pick overrides (mainly
 * for use in the disk editor).  Not sure if we want the changes to "stick";
 * we probably do.  Q: does the "scan for sub-volumes" attribute propagate
 * recursively to each sub-sub-volume? Probably.
 *
 * NOTE to self: should make "test" functions more lax when called
 * from here, on the assumption that the caller is knowledgeable.  Perhaps
 * an independent "strictness" variable that can be cranked down through
 * multiple calls to AnalyzeImage??
 */
class DISKIMG_API DiskFS {
public:
    /*
     * Information about volume usage.
     *
     * Each "chunk" is a track/sector on a DOS disk or a block on a ProDOS
     * or Pascal disk.  CP/M really ought to use 1K blocks, but for
     * convenience we're just using 512-byte blocks (it's up to the CP/M
     * code to set two "chunks" per block).
     *
     * NOTE: the current DOS/ProDOS/Pascal code is sloppy when it comes to
     * keeping this structure up to date.  HFS doesn't use it at all.  This
     * has always been a low-priority feature.
     */
    class DISKIMG_API VolumeUsage {
    public:
        VolumeUsage(void) {
            fByBlocks = false;
            fTotalChunks = -1;
            fNumSectors = -1;
            //fFreeChunks = -1;
            fList = NULL;
            fListSize = -1;
        }
        ~VolumeUsage(void) {
            delete[] fList;
        }

        /*
         * These values MUST fit in five bits.
         *
         * Suggested disk map colors:
         *  0 = unknown (color-key pink)
         *  1 = conflict (medium-red)
         *  2 = boot loader (dark-blue)
         *  3 = volume dir (light-blue)
         *  4 = subdir (medium-blue)
         *  5 = user data (medium-green)
         *  6 = user index blocks (light-green)
         *  7 = embedded filesys (yellow)
         *
         * THOUGHT: Add flag for I/O error (nibble images) -- requires
         * automatic disk verify pass.  (Or maybe could be done manually
         * on request?)
         *
         * unused --> black
         * marked-used-but-not-used --> dark-red
         * used-but-not-marked-used --> orange
         */
        typedef enum {
            kChunkPurposeUnknown    = 0,
            kChunkPurposeConflict   = 1,    // two or more different things
            kChunkPurposeSystem     = 2,    // boot blocks, volume bitmap
            kChunkPurposeVolumeDir  = 3,    // volume dir (or only dir)
            kChunkPurposeSubdir     = 4,    // ProDOS sub-directory
            kChunkPurposeUserData   = 5,    // file on this filesystem
            kChunkPurposeFileStruct = 6,    // index blocks, T/S lists
            kChunkPurposeEmbedded   = 7,    // embedded filesystem
            // how about: outside range claimed by disk, e.g. fTotalBlocks on
            //  800K ProDOS disk in a 32MB CFFA volume?
        } ChunkPurpose;

        typedef struct ChunkState {
            bool            isUsed;
            bool            isMarkedUsed;
            ChunkPurpose    purpose;    // only valid if isUsed is set
        } ChunkState;

        // initialize, configuring for either blocks or sectors
        DIError Create(long numBlocks);
        DIError Create(long numTracks, long numSectors);
        bool GetInitialized(void) const { return (fList != NULL); }

        // return the number of chunks on this disk
        long GetNumChunks(void) const { return fTotalChunks; }

        // return the number of unallocated chunks, taking into account
        //  both the free-chunk bitmap (if any) and the actual usage
        long GetActualFreeChunks(void) const;

        // return the state of the specified chunk
        DIError GetChunkState(long block, ChunkState* pState) const;
        DIError GetChunkState(long track, long sector,
            ChunkState* pState) const;

        // set the state of a particular chunk (should only be done by
        //  the DiskFS sub-classes)
        DIError SetChunkState(long block, const ChunkState* pState);
        DIError SetChunkState(long track, long sector,
            const ChunkState* pState);

        void Dump(void) const;  // debugging

    private:
        DIError GetChunkStateIdx(int idx, ChunkState* pState) const;
        DIError SetChunkStateIdx(int idx, const ChunkState* pState);
        inline char StateToChar(ChunkState* pState) const;

        /*
         * Chunk state is stored as a set of bits in one byte:
         *
         *  0-4: how is block used (only has meaning if bit 6 is set)
         *  5: for nibble images, indicates the block or sector is unreadable
         *  6: is block used by something (0=no, 1=yes)
         *  7: is block marked "in use" by system map (0=no, 1=yes)
         *
         * [ Consider reducing "purpose" to 0-3 and adding bad block bit for
         *   nibble images and physical media.]
         */
        enum {
            kChunkPurposeMask       = 0x1f, // ChunkPurpose enum
            kChunkDamagedFlag       = 0x20,
            kChunkUsedFlag          = 0x40,
            kChunkMarkedUsedFlag    = 0x80,
        };

        bool        fByBlocks;
        long        fTotalChunks;
        long        fNumSectors;        // only valid if !fByBlocks
        //long      fFreeChunks;
        uint8_t*    fList;
        int         fListSize;
    };  // end of VolumeUsage class

    /*
     * List of sub-volumes.  The SubVolume owns the DiskImg and DiskFS
     * that are handed to it, so they can be deleted when the SubVolume
     * is deleted as part of destroying the parent.
     */
    class SubVolume {
    public:
        SubVolume(void) : fpDiskImg(NULL), fpDiskFS(NULL),
            fpPrev(NULL), fpNext(NULL) {}
        ~SubVolume(void) {
            delete fpDiskFS;    // must close first; may need flush to DiskImg
            delete fpDiskImg;
        }

        void Create(DiskImg* pDiskImg, DiskFS* pDiskFS) {
            assert(pDiskImg != NULL);
            assert(pDiskFS != NULL);
            fpDiskImg = pDiskImg;
            fpDiskFS = pDiskFS;
        }

        DiskImg* GetDiskImg(void) const { return fpDiskImg; }
        DiskFS* GetDiskFS(void) const { return fpDiskFS; }

        SubVolume* GetPrev(void) const { return fpPrev; }
        void SetPrev(SubVolume* pSubVol) { fpPrev = pSubVol; }
        SubVolume* GetNext(void) const { return fpNext; }
        void SetNext(SubVolume* pSubVol) { fpNext = pSubVol; }

    private:
        DiskImg*    fpDiskImg;
        DiskFS*     fpDiskFS;

        SubVolume*  fpPrev;
        SubVolume*  fpNext;
    };  // end of SubVolume class



    /*
     * Start of DiskFS declarations.
     */
public:
    typedef enum SubScanMode {
        kScanSubUnknown = 0,
        kScanSubDisabled,
        kScanSubEnabled,
        kScanSubContainerOnly,
    } SubScanMode;


    DiskFS(void) {
        fpA2Head = fpA2Tail = NULL;
        fpSubVolumeHead = fpSubVolumeTail = NULL;
        fpImg = NULL;
        fScanForSubVolumes = kScanSubDisabled;

        fParmTable[kParm_CreateUnique] = 0;
        fParmTable[kParmProDOS_AllowLowerCase] = 1;
        fParmTable[kParmProDOS_AllocSparse] = 1;
    }
    virtual ~DiskFS(void) {
        DeleteSubVolumeList();
        DeleteFileList();
        SetDiskImg(NULL);
    }

    /*
     * Static FSFormat-analysis functions, called by the DiskImg AnalyzeImage
     * function.  Capable of determining with a high degree of accuracy
     * what format the disk is in, yet remaining flexible enough to
     * function correctly with variations (like DOS3.3 disks with
     * truncated TOCs and ProDOS images from hard drives).
     *
     * The "pOrder" and "pFormat" arguments are in/out.  Set them to the
     * appropriate "unknown" enum value on entry.  If something is known
     * of the order or format, put that in instead; in some cases this will
     * bias the proceedings, which is useful for efficiency and for making
     * overrides work correctly.
     *
     * On success, these return kDIErrNone and set "*pOrder".  On failure,
     * they return nonzero and leave "*pOrder" unmodified.
     *
     * Each DiskFS sub-class should declare a TestFS function.  It's not
     * virtual void here because, since it's called before the DiskFS is
     * created, it must be static.
     */
    typedef enum FSLeniency { kLeniencyNot, kLeniencyVery } FSLeniency;
    //static DIError TestFS(const DiskImg* pImg, DiskImg::SectorOrder* pOrder,
    //  DiskImg::FSFormat* pFormat, FSLeniency leniency);

    /*
     * Load the disk contents and (if enabled) scan for sub-volumes.
     *
     * If "headerOnly" is set, we just do a quick scan of the volume header
     * to get basic information.  The deep file scan is skipped (but can
     * be done later).  (Sub-classes can choose to ignore the flag and
     * always do the full scan; this is an optimization.)  Guaranteed to
     * set the volume name and volume block/sector count.
     *
     * If a progress callback is set up, this can return with a "cancelled"
     * result, which should not be treated as a failure.
     */
    typedef enum { kInitUnknown = 0, kInitHeaderOnly, kInitFull } InitMode;
    virtual DIError Initialize(DiskImg* pImg, InitMode initMode) = 0;

    /*
     * Format the disk with the appropriate filesystem, creating all filesystem
     * structures and (when appropriate) boot blocks.
     */
    virtual DIError Format(DiskImg* pDiskImg, const char* volName)
    { return kDIErrNotSupported; }

    /*
     * Pass in a full path to normalize, and a buffer to copy the output
     * into.  On entry "pNormalizedBufLen" holds the length of the buffer.
     * On exit, it holds the size of the buffer required to hold the
     * normalized string.  If the buffer is NULL or isn't big enough, no part
     * of the normalized path will be copied into the buffer, and a specific
     * error (kDIErrDataOverrun) will be returned.
     *
     * Generally speaking, the normalization process involves separating
     * the pathname into its components, modifying each filename as needed
     * to make it legal on the current filesystem, and then reassembling
     * the components.
     *
     * The input and output strings are Mac OS Roman.
     */
    virtual DIError NormalizePath(const char* path, char fssep,
        char* normalizedBuf, int* pNormalizedBufLen)
    { return kDIErrNotSupported; }


    /*
     * Create a file.  The CreateParms struct specifies the full set of file
     * details.  To remain FS-agnostic, use the NufxLib constants
     * (kNuStorageDirectory, kNuAccessUnlocked, etc).  They match up with
     * their ProDOS equivalents, and I promise to make them work right.
     *
     * On success, the file exists as a fully-formed, zero-length file.  A
     * pointer to the relevant A2File structure is returned.
     */
    enum {
        /* valid values for CreateParms; must match ProDOS enum */
        kStorageSeedling        = 1,
        kStorageExtended        = 5,
        kStorageDirectory       = 13,
    };
    typedef struct CreateParms {
        const char*     pathName;       // full pathname for file on disk image
        char            fssep;
        int             storageType;    // determines normal, subdir, or forked
        uint32_t        fileType;
        uint32_t        auxType;
        uint32_t        access;
        time_t          createWhen;
        time_t          modWhen;
    } CreateParms;
    virtual DIError CreateFile(const CreateParms* pParms, A2File** ppNewFile)
        { return kDIErrNotSupported; }

    /*
     * Delete a file from the disk.
     */
    virtual DIError DeleteFile(A2File* pFile)
        { return kDIErrNotSupported; }

    /*
     * Rename a file.
     */
    virtual DIError RenameFile(A2File* pFile, const char* newName)
        { return kDIErrNotSupported; }

    /*
     * Alter file attributes.
     */
    virtual DIError SetFileInfo(A2File* pFile, uint32_t fileType,
            uint32_t auxType, uint32_t accessFlags)
        { return kDIErrNotSupported; }

    /*
     * Rename a volume.  Also works for changing the disk volume number.
     */
    virtual DIError RenameVolume(const char* newName)
        { return kDIErrNotSupported; }


    // Accessor
    DiskImg* GetDiskImg(void) const { return fpImg; }

    // Test file and volume names (and volume numbers)
    // [these need to be static functions for some things... hmm]
    //virtual bool IsValidFileName(const char* name) const { return false; }
    //virtual bool IsValidVolumeName(const char* name) const { return false; }

    // Return the disk volume name, or NULL if there isn't one.
    virtual const char* GetVolumeName(void) const = 0;

    // Return a printable string identifying the FS type and volume
    virtual const char* GetVolumeID(void) const = 0;

    // Return the "bare" volume name.  For formats where the volume name
    // is actually a number (e.g. DOS 3.3), this returns just the number.
    // For formats without a volume name or number (e.g. CP/M), this returns
    // NULL, indicating that any attempt to change the volume name will fail.
    virtual const char* GetBareVolumeName(void) const = 0;

    // Returns "false" if we only support read-only access to this FS type
    virtual bool GetReadWriteSupported(void) const = 0;

    // Returns "true" if the filesystem shows evidence of damage.
    virtual bool GetFSDamaged(void) const = 0;

    // Returns number of blocks recognized by the filesystem, or -1 if the
    // FS isn't block-oriented (e.g. DOS 3.2/3.3)
    virtual long GetFSNumBlocks(void) const { return -1; }

    // Get the next file in the list.  Start by passing in NULL to get the
    //  head of the list.  Returns NULL when the end of the list is reached.
    A2File* GetNextFile(A2File* pCurrent) const;

    // Get a count of the files and directories on this disk.
    long GetFileCount(void) const;

    /*
     * Find a file by case-insensitive pathname.  Assumes fssep=':'.  The
     * compare function can be overridden for systems like HFS, where "case
     * insensitive" has a different meaning because of the native
     * character set.
     *
     * The A2File* returned should not be deleted.
     */
    typedef int (*StringCompareFunc)(const char* str1, const char* str2);
    A2File* GetFileByName(const char* pathName, StringCompareFunc func = NULL);

    // This controls scanning for sub-volumes; must be set before Initialize().
    SubScanMode GetScanForSubVolumes(void) const { return fScanForSubVolumes; }
    void SetScanForSubVolumes(SubScanMode val) { fScanForSubVolumes = val; }

    // some constants for non-ProDOS filesystems to use
    enum { kFileAccessLocked = 0x01, kFileAccessUnlocked = 0xc3 };


    /*
     * We use this as a filename separator character (i.e. between pathname
     * components) in all filenames.  It's useful to standardize on this
     * so that behavior is consistent across all disk and archive formats.
     *
     * The choice of ':' is good because it's invalid in filenames on
     * Windows, Mac OS, GS/OS, and pretty much anywhere else we could be
     * running except for UNIX.  It's valid under DOS 3.3, but since you
     * can't have subdirectories under DOS there's no risk of confusion.
     */
    enum { kDIFssep = ':' };


    /*
     * Return the volume use map.  This is a non-const function because
     * it might need to do a "just-in-time" usage map update.  It returns
     * const to keep non-DiskFS classes from altering the map.
     */
    const VolumeUsage* GetVolumeUsageMap(void) {
        if (fVolumeUsage.GetInitialized())
            return &fVolumeUsage;
        else
            return NULL;
    }

    /*
     * Return the total space and free space, in either blocks or sectors
     * as appropriate.  "*pUnitSize" will be kBlockSize or kSectorSize.
     */
    virtual DIError GetFreeSpaceCount(long* pTotalUnits, long* pFreeUnits,
        int* pUnitSize) const = 0;


    /*
     * Get the next volume in the list.  Start by passing in NULL to get the
     * head of the list.  Returns NULL when the end of the list is reached.
     */
    SubVolume* GetNextSubVolume(const SubVolume* pCurrent) const;

    /*
     * Set some parameters to tell the DiskFS how to operate.  Some of
     * these are FS-specific, some may be general.
     *
     * Parameters are set in the current object and all sub-volume objects.
     *
     * The enum is part of the interface and must be rigidly defined, but
     * it is also used to size an array so it can't be too sparse.
     */
    typedef enum DiskFSParameter {
        kParmUnknown = 0,

        kParm_CreateUnique = 1,             // make new filenames unique

        kParmProDOS_AllowLowerCase = 10,    // allow lower case and spaces
        kParmProDOS_AllocSparse = 11,       // don't store empty blocks

        kParmMax        // must be last entry
    } DiskFSParameter;
    long GetParameter(DiskFSParameter parm);
    void SetParameter(DiskFSParameter parm, long val);

    /*
     * Flush changed data.
     *
     * The individual filesystems shouldn't generally do any caching; if
     * they do, we would want a virtual "FlushFS()" that gets called by
     * Flush.  The better answer is to cache in DiskImg, which works for
     * everything, and knows if the underlying storage is already in RAM.
     *
     * For the most part this just needs to recursively flush the DiskImg
     * objects in all of the sub-volumes and then the current volume.  This
     * is a no-op in most cases, but if the archive is compressed this will
     * cause a new, compressed archive to be created.
     *
     * The "mode" value determines whether or not we do "heavy" flushes.  It's
     * very handy to be able to do "slow" flushes for anything that is being
     * written directly to disk (as opposed to being run through Deflate),
     * so that the UI doesn't indicate that they're partially written when
     * in fact they're fully written.
     */
    DIError Flush(DiskImg::FlushMode mode);

    /*
     * Set the read-only flag on our DiskImg and those of our subvolumes.
     * Used to ensure that a DiskFS with un-flushed data can be deleted
     * without corrupting the volume.
     */
    void SetAllReadOnly(bool val);

    // debug dump
    void DumpFileList(void);

protected:
    /*
     * Set the DiskImg pointer.  Updates the reference count in DiskImg.
     */
    void SetDiskImg(DiskImg* pImg);

    // once added, we own the pDiskImg and the pDiskFS (DO NOT pass the
    //  same DiskImg or DiskFS in more than once!).  Note this copies the
    //  fParmTable and other stuff (fScanForSubVolumes) from parent to child.
    void AddSubVolumeToList(DiskImg* pDiskImg, DiskFS* pDiskFS);
    // add files to fpA2Head/fpA2Tail
    void AddFileToList(A2File* pFile);
    // only need for hierarchical filesystems; insert file after pPrev
    void InsertFileInList(A2File* pFile, A2File* pPrev);
    // delete an entry
    void DeleteFileFromList(A2File* pFile);

    // scan for damaged or suspicious files
    void ScanForDamagedFiles(bool* pDamaged, bool* pSuspicious);

    // pointer to the DiskImg structure underlying this filesystem
    DiskImg*    fpImg;

    VolumeUsage fVolumeUsage;
    SubScanMode fScanForSubVolumes;


private:
    A2File* SkipSubdir(A2File* pSubdir);
    void CopyInheritables(DiskFS* pNewFS);
    void DeleteFileList(void);
    void DeleteSubVolumeList(void);

    long fParmTable[kParmMax];          // for DiskFSParameter

    A2File*     fpA2Head;
    A2File*     fpA2Tail;
    SubVolume*  fpSubVolumeHead;
    SubVolume*  fpSubVolumeTail;

private:
    DiskFS& operator=(const DiskFS&);
    DiskFS(const DiskFS&);
};


/*
 * Apple II file class, representing a file on an Apple II volume.  This is an
 * abstract base class.
 *
 * There is a different sub-class for each filesystem type.  The A2File object
 * encapsulates all of the knowledge required to read a file from a disk
 * image.
 *
 * The prev/next pointers, used to maintain a linked list of files, are only
 * accessible from DiskFS functions.  At some point we may want to rearrange
 * the way this is handled, e.g. by not maintaining a list at all, so it's
 * important that everything go through DiskFS requests.
 *
 * The FSFormat is made an explicit member, because sub-classes may not
 * understand exactly where the file came from (e.g. was it DOS3.2 or
 * DOS 3.3).  Somebody might care.
 *
 *
 * NOTE TO SELF: open files need to be generalized.  Right now the internal
 * implementations only allow a single open, which is okay for our purposes
 * but bad for a general FS implementation.  As it stands, you can't even
 * open both forks at the same time on ProDOS/HFS.
 *
 * UMMM: The handling of "damaged" files could be more consistent.
 */
class DISKIMG_API A2File {
public:
    friend class DiskFS;

    A2File(DiskFS* pDiskFS) : fpDiskFS(pDiskFS) {
        fpPrev = fpNext = NULL;
        fFileQuality = kQualityGood;
    }
    virtual ~A2File(void) {}

    /*
     * All Apple II files have certain characteristics, of which ProDOS
     * is roughly a superset.  (Yes, you can have HFS on a IIgs, but
     * all that fancy creator type stuff is decidedly Mac-centric.  Still,
     * we want to assume 4-byte file and aux types.)
     *
     * NEED: something distinguishing between files and disk images?
     *
     * NOTE: there is no guarantee that GetPathName will return unique values
     * (duplicates are possible).  We don't guarantee that you won't get an
     * empty string back (it's valid to have an empty filename in the dir in
     * DOS 3.3, and it's possible for other filesystems to be damaged).
     *
     * The filename returned is defined to be null-terminated Mac OS Roman.
     * For most filesystems this is automatic, as filenames are restricted
     * to ASCII, but for DOS 3.3 it requires stripping high bits.  It also
     * means that embedded nulls in HFS filenames (which are discouraged but
     * allowed) will be lost.
     *
     * We do guarantee that the contents of subdirectories are grouped
     * together.  This makes it much easier to construct a hierarchy out of
     * the linear list.  This becomes important when dynamically adding
     * files to a disk.
     */
    virtual const char* GetFileName(void) const = 0;    // name of this file
    virtual const char* GetPathName(void) const = 0;    // full path
    virtual char GetFssep(void) const = 0;              // '\0' if none
    virtual uint32_t GetFileType(void) const = 0;
    virtual uint32_t GetAuxType(void) const = 0;
    virtual uint32_t GetAccess(void) const = 0;         // ProDOS-style perms
    virtual time_t GetCreateWhen(void) const = 0;
    virtual time_t GetModWhen(void) const = 0;
    virtual di_off_t GetDataLength(void) const = 0;     // len of data fork
    virtual di_off_t GetDataSparseLength(void) const = 0;   // len w/o sparse areas
    virtual di_off_t GetRsrcLength(void) const = 0;     // len or -1 if no rsrc
    virtual di_off_t GetRsrcSparseLength(void) const = 0;   // len or -1 if no rsrc
    virtual DiskImg::FSFormat GetFSFormat(void) const {
        return fpDiskFS->GetDiskImg()->GetFSFormat();
    }
    virtual bool IsDirectory(void) const { return false; }
    virtual bool IsVolumeDirectory(void) const { return false; }

    /*
     * Open a file.  Treat the A2FileDescr like an fd.
     */
    virtual DIError Open(A2FileDescr** ppOpenFile, bool readOnly,
        bool rsrcFork = false) = 0;

    /*
     * This is called by the A2FileDescr object when somebody invokes Close().
     * The A2File object should remove the A2FileDescr from its list of open
     * files and delete the storage.  The implementation must not call the
     * A2FileDescr's Close function, since that would cause a recursive loop.
     *
     * This should not be called by an application.
     */
    virtual void CloseDescr(A2FileDescr* pOpenFile) = 0;

    /*
     * This is only useful for hierarchical filesystems like ProDOS,
     * where the order of items in the linear list is significant.  It
     * allows an unambiguous determination of which subdir a file resides
     * in, even if somebody has sector-edited the filesystem so that two
     * subdirs have the same name.  (It's also a bit speedier to compare
     * than pathname substrings would be.)
     */
    virtual A2File* GetParent(void) const { return NULL; }

    /*
     * Returns "true" if either fork of the file is open, "false" if not.
     */
    virtual bool IsFileOpen(void) const = 0;

    virtual void Dump(void) const = 0;      // debugging

    typedef enum FileQuality {
        kQualityUnknown = 0,
        kQualityGood,
        kQualitySuspicious,
        kQualityDamaged,
    } FileQuality;
    virtual FileQuality GetQuality(void) const { return fFileQuality; }
    virtual void SetQuality(FileQuality quality);
    virtual void ResetQuality(void);

    DiskFS* GetDiskFS(void) const { return fpDiskFS; }

protected:
    DiskFS*     fpDiskFS;
    virtual void SetParent(A2File* pParent) { /* do nothing */ }

private:
    A2File* GetPrev(void) const { return fpPrev; }
    void SetPrev(A2File* pFile) { fpPrev = pFile; }
    A2File* GetNext(void) const { return fpNext; }
    void SetNext(A2File* pFile) { fpNext = pFile; }

    // Set when file structure is damaged and application should not try
    //  to open the file.
    FileQuality fFileQuality;

    A2File*     fpPrev;
    A2File*     fpNext;


private:
    A2File& operator=(const A2File&);
    A2File(const A2File&);
};


/*
 * Abstract representation of an open file.  This contains all active state
 * and all information required to read and write a file.
 *
 * Do not delete these objects; instead, invoke the Close method, so that they
 * can be removed from the parents' list of open files.
 * TODO: consider making the destructor "protected"
 */
class DISKIMG_API A2FileDescr {
public:
    A2FileDescr(A2File* pFile) : fpFile(pFile), fProgressUpdateFunc(NULL) {}
    virtual ~A2FileDescr(void) { fpFile = NULL; /*paranoia*/ }

    virtual DIError Read(void* buf, size_t len, size_t* pActual = NULL) = 0;
    virtual DIError Write(const void* buf, size_t len, size_t* pActual = NULL) = 0;
    virtual DIError Seek(di_off_t offset, DIWhence whence) = 0;
    virtual di_off_t Tell(void) = 0;
    virtual DIError Close(void) = 0;

    virtual DIError GetStorage(long sectorIdx, long* pTrack, long* pSector)
        const = 0;
    virtual DIError GetStorage(long blockIdx, long* pBlock)
        const = 0;
    virtual long GetSectorCount(void) const = 0;
    virtual long GetBlockCount(void) const = 0;

    virtual DIError Rewind(void) { return Seek(0, kSeekSet); }

    A2File* GetFile(void) const { return fpFile; }

    /*
     * Progress update callback mechanism.  Pass in the length or (for writes)
     * expected length of the file.  This invokes the callback with the
     * lengths and some pointers.
     *
     * If the progress callback returns "true", progress continues.  If it
     * returns "false", the read or write function will return with
     * kDIErrCancelled.
     */
    typedef bool (*ProgressUpdater)(A2FileDescr* pFile, di_off_t max,
        di_off_t current, void* vState);
    void SetProgressUpdater(ProgressUpdater func, di_off_t max, void* state) {
        fProgressUpdateFunc = func;
        fProgressUpdateMax = max;
        fProgressUpdateState = state;
    }
    void ClearProgressUpdater(void) {
        fProgressUpdateFunc = NULL;
    }

protected:
    A2File*     fpFile;

    /*
     * Internal utility functions for mapping blocks to sectors and vice-versa.
     */
    virtual void TrackSectorToBlock(long track, long sector, long* pBlock,
        bool* pSecondHalf) const
    {
        int numSectPerTrack = fpFile->GetDiskFS()->GetDiskImg()->GetNumSectPerTrack();
        assert(track < fpFile->GetDiskFS()->GetDiskImg()->GetNumTracks());
        assert(sector < numSectPerTrack);
        long dblBlock = track * numSectPerTrack + sector;
        *pBlock = dblBlock / 2;
        *pSecondHalf = (dblBlock & 0x01) != 0;
    }
    virtual void BlockToTrackSector(long block, bool secondHalf, long* pTrack,
        long* pSector) const
    {
        assert(block < fpFile->GetDiskFS()->GetDiskImg()->GetNumBlocks());
        int numSectPerTrack = fpFile->GetDiskFS()->GetDiskImg()->GetNumSectPerTrack();
        int dblBlock = block * 2;
        if (secondHalf)
            dblBlock++;
        *pTrack = dblBlock / numSectPerTrack;
        *pSector = dblBlock % numSectPerTrack;
    }

    /*
     * Call this from FS-specific read/write functions on successful
     * completion (and perhaps more often for filesystems with potentially
     * large files, e.g. ProDOS/HFS).
     *
     * Test the return value; if "false", user wishes to cancel the op, and
     * long read or write calls should return immediately.
     */
    inline bool UpdateProgress(di_off_t current) {
        if (fProgressUpdateFunc != NULL) {
            return (*fProgressUpdateFunc)(this, fProgressUpdateMax, current,
                        fProgressUpdateState);
        } else {
            return true;
        }
    }

private:
    A2FileDescr& operator=(const A2FileDescr&);
    A2FileDescr(const A2FileDescr&);

    /* storage for progress update goodies */
    ProgressUpdater     fProgressUpdateFunc;
    di_off_t            fProgressUpdateMax;
    void*               fProgressUpdateState;
};

}   // namespace DiskImgLib

#endif /*DISKIMG_DISKIMG_H*/
