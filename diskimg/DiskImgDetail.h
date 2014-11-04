/*
 * CiderPress
 * Copyright (C) 2009 by CiderPress authors.  All Rights Reserved.
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Sub-classes of the base classes defined in DiskImg.h.
 *
 * Most applications will not need to include this file, because the
 * polymorphic interfaces do everything they need.  If something needs to
 * examine the actual directory structure of a file, it can do so through
 * these declarations.
 */
#ifndef __DISKIMGDETAIL__
#define __DISKIMGDETAIL__

#include "../prebuilt/NufxLib.h"
#define ZLIB_DLL
#include "../prebuilt/zlib.h"

#include "DiskImg.h"

#ifndef EXCISE_GPL_CODE
# include "libhfs/hfs.h"
#endif

namespace DiskImgLib {

/*
 * ===========================================================================
 *      Outer wrappers
 * ===========================================================================
 */

/*
 * Outer wrapper class, representing a compression utility or archive
 * format that must be stripped away so we can get to the Apple II stuff.
 *
 * Outer wrappers usually have a filename embedded in them, representing
 * the original name of the file.  We want to use the extension from this
 * name when evaluating the file contents.  Usually.
 */
class OuterWrapper {
public:
    OuterWrapper(void) {}
    virtual ~OuterWrapper(void) {}

    // all sub-classes should have one of these
    //static DIError Test(GenericFD* pGFD, long outerLength);

    // open the file and prepare to access it; fills out return values
    // NOTE: pGFD must be a GFDFile.
    virtual DIError Load(GenericFD* pOuterGFD, di_off_t outerLength, bool readOnly,
        di_off_t* pWrapperLength, GenericFD** ppWrapperGFD) = 0;

    virtual DIError Save(GenericFD* pOuterGFD, GenericFD* pWrapperGFD,
        di_off_t wrapperLength) = 0;

    // set on recoverable errors, like a CRC failure
    virtual bool IsDamaged(void) const = 0;

    // indicate that we don't have a "fast" flush
    virtual bool HasFastFlush(void) const { return false; }

    virtual const char* GetExtension(void) const = 0;

private:
    OuterWrapper& operator=(const OuterWrapper&);
    OuterWrapper(const OuterWrapper&);
};

class OuterGzip : public OuterWrapper {
public:
    OuterGzip(void) { fWrapperDamaged = false; }
    virtual ~OuterGzip(void) {}

    static DIError Test(GenericFD* pGFD, di_off_t outerLength);
    virtual DIError Load(GenericFD* pGFD, di_off_t outerLength, bool readOnly,
        di_off_t* pTotalLength, GenericFD** ppNewGFD);
    virtual DIError Save(GenericFD* pOuterGFD, GenericFD* pWrapperGFD,
        di_off_t wrapperLength);

    virtual bool IsDamaged(void) const { return fWrapperDamaged; }

    virtual const char* GetExtension(void) const { return NULL; }

private:
    DIError ExtractGzipImage(gzFile gzfp, char** pBuf, di_off_t* pLength);
    DIError CloseGzip(void);

    // Largest possible ProDOS volume; quite a bit to hold in RAM. Add a
    // little extra for .hdv format.
    enum { kMaxUncompressedSize = kGzipMax +256 };

    bool    fWrapperDamaged;
};

class OuterZip : public OuterWrapper {
public:
    OuterZip(void) : fStoredFileName(NULL), fExtension(NULL) {}
    virtual ~OuterZip(void) {
        delete[] fStoredFileName;
        delete[] fExtension;
    }

    static DIError Test(GenericFD* pGFD, di_off_t outerLength);
    virtual DIError Load(GenericFD* pGFD, di_off_t outerLength, bool readOnly,
        di_off_t* pTotalLength, GenericFD** ppNewGFD);
    virtual DIError Save(GenericFD* pOuterGFD, GenericFD* pWrapperGFD,
        di_off_t wrapperLength);

    virtual bool IsDamaged(void) const { return false; }

    virtual const char* GetExtension(void) const { return fExtension; }

private:
    class LocalFileHeader {
    public:
        LocalFileHeader(void) :
            fVersionToExtract(0),
            fGPBitFlag(0),
            fCompressionMethod(0),
            fLastModFileTime(0),
            fLastModFileDate(0),
            fCRC32(0),
            fCompressedSize(0),
            fUncompressedSize(0),
            fFileNameLength(0),
            fExtraFieldLength(0),
            fFileName(NULL)
        {}
        virtual ~LocalFileHeader(void) { delete[] fFileName; }

        DIError Read(GenericFD* pGFD);
        DIError Write(GenericFD* pGFD);
        void SetFileName(const char* name);

        // unsigned long fSignature;
        unsigned short  fVersionToExtract;
        unsigned short  fGPBitFlag;
        unsigned short  fCompressionMethod;
        unsigned short  fLastModFileTime;
        unsigned short  fLastModFileDate;
        unsigned long   fCRC32;
        unsigned long   fCompressedSize;
        unsigned long   fUncompressedSize;
        unsigned short  fFileNameLength;
        unsigned short  fExtraFieldLength;
        unsigned char*  fFileName;
        // extra field

        enum {
            kSignature      = 0x04034b50,
            kLFHLen         = 30,       // LocalFileHdr len, excl. var fields
        };

        void Dump(void) const;
    };

    class CentralDirEntry {
    public:
        CentralDirEntry(void) :
            fVersionMadeBy(0),
            fVersionToExtract(0),
            fGPBitFlag(0),
            fCompressionMethod(0),
            fLastModFileTime(0),
            fLastModFileDate(0),
            fCRC32(0),
            fCompressedSize(0),
            fUncompressedSize(0),
            fFileNameLength(0),
            fExtraFieldLength(0),
            fFileCommentLength(0),
            fDiskNumberStart(0),
            fInternalAttrs(0),
            fExternalAttrs(0),
            fLocalHeaderRelOffset(0),
            fFileName(NULL),
            fFileComment(NULL)
        {}
        virtual ~CentralDirEntry(void) {
            delete[] fFileName;
            delete[] fFileComment;
        }

        DIError Read(GenericFD* pGFD);
        DIError Write(GenericFD* pGFD);
        void SetFileName(const char* name);

        // unsigned long fSignature;
        unsigned short  fVersionMadeBy;
        unsigned short  fVersionToExtract;
        unsigned short  fGPBitFlag;
        unsigned short  fCompressionMethod;
        unsigned short  fLastModFileTime;
        unsigned short  fLastModFileDate;
        unsigned long   fCRC32;
        unsigned long   fCompressedSize;
        unsigned long   fUncompressedSize;
        unsigned short  fFileNameLength;
        unsigned short  fExtraFieldLength;
        unsigned short  fFileCommentLength;
        unsigned short  fDiskNumberStart;
        unsigned short  fInternalAttrs;
        unsigned long   fExternalAttrs;
        unsigned long   fLocalHeaderRelOffset;
        unsigned char*  fFileName;
        // extra field
        unsigned char*  fFileComment;   // alloc with new[]

        void Dump(void) const;

        enum {
            kSignature      = 0x02014b50,
            kCDELen         = 46,       // CentralDirEnt len, excl. var fields
        };
    };

    class EndOfCentralDir {
    public:
        EndOfCentralDir(void) :
            fDiskNumber(0),
            fDiskWithCentralDir(0),
            fNumEntries(0),
            fTotalNumEntries(0),
            fCentralDirSize(0),
            fCentralDirOffset(0),
            fCommentLen(0)
            {}
        virtual ~EndOfCentralDir(void) {}

        DIError ReadBuf(const unsigned char* buf, int len);
        DIError Write(GenericFD* pGFD);

        // unsigned long fSignature;
        unsigned short  fDiskNumber;
        unsigned short  fDiskWithCentralDir;
        unsigned short  fNumEntries;
        unsigned short  fTotalNumEntries;
        unsigned long   fCentralDirSize;
        unsigned long   fCentralDirOffset;      // offset from first disk
        unsigned short  fCommentLen;
        // archive comment

        enum {
            kSignature      = 0x06054b50,
            kEOCDLen        = 22,       // EndOfCentralDir len, excl. comment
        };

        void Dump(void) const;
    };

    enum {
        kDataDescriptorSignature    = 0x08074b50,

        kMaxCommentLen  = 65535,        // longest possible in ushort
        kMaxEOCDSearch  = kMaxCommentLen + EndOfCentralDir::kEOCDLen,

        kZipFssep       = '/',
        kDefaultVersion = 20,
        kMaxUncompressedSize = kGzipMax +256,
    };
    enum {
        kCompressStored     = 0,        // no compression
        //kCompressShrunk       = 1,
        //kCompressImploded = 6,
        kCompressDeflated   = 8,        // standard deflate
    };

    static DIError ReadCentralDir(GenericFD* pGFD, di_off_t outerLength,
        CentralDirEntry* pDirEntry);
    DIError ExtractZipEntry(GenericFD* pOuterGFD, CentralDirEntry* pCDE,
        unsigned char** pBuf, di_off_t* pLength);
    DIError InflateGFDToBuffer(GenericFD* pGFD, unsigned long compSize,
        unsigned long uncompSize, unsigned char* buf);
    DIError DeflateGFDToGFD(GenericFD* pDst, GenericFD* pSrc, di_off_t length,
        di_off_t* pCompLength, unsigned long* pCRC);

private:
    void SetExtension(const char* ext);
    void SetStoredFileName(const char* name);
    void GetMSDOSTime(unsigned short* pDate, unsigned short* pTime);
    void DOSTime(time_t when, unsigned short* pDate, unsigned short* pTime);

    char*       fStoredFileName;
    char*       fExtension;
};


/*
 * ===========================================================================
 *      Image wrappers
 * ===========================================================================
 */

/*
 * Image wrapper class, representing the format of the Windows files.
 * Might be "raw" data, might be data with a header, might be a complex
 * or compressed format that must be extracted to a buffer.
 */
class ImageWrapper {
public:
    ImageWrapper(void) {}
    virtual ~ImageWrapper(void) {}

    // all sub-classes should have one of these
    // static DIError Test(GenericFD* pGFD, di_off_t wrappedLength);

    // open the file and prepare to access it; fills out return values
    virtual DIError Prep(GenericFD* pGFD, di_off_t wrappedLength, bool readOnly,
        di_off_t* pLength, DiskImg::PhysicalFormat* pPhysical,
        DiskImg::SectorOrder* pOrder, short* pDiskVolNum,
        LinearBitmap** ppBadBlockMap, GenericFD** ppNewGFD) = 0;

    // fill out the wrapper, using the specified parameters
    virtual DIError Create(di_off_t length, DiskImg::PhysicalFormat physical,
        DiskImg::SectorOrder order, short dosVolumeNum, GenericFD* pWrapperGFD,
        di_off_t* pWrappedLength, GenericFD** pDataFD) = 0;

    // push altered data to the wrapper GFD
    virtual DIError Flush(GenericFD* pWrapperGFD, GenericFD* pDataGFD,
        di_off_t dataLen, di_off_t* pWrappedLen) = 0;

    // set the storage name (used by some formats)
    virtual void SetStorageName(const char* name) {
        // default implementation
        assert(false);
    }

    // indicate that we have a "fast" flush
    virtual bool HasFastFlush(void) const = 0;

    // set by "Prep" on recoverable errors, like a CRC failure, for some fmts
    virtual bool IsDamaged(void) const { return false; }

    // if this wrapper format includes a file comment, return it
    //virtual const char* GetComment(void) const { return NULL; }

    /*
     * Some additional goodies required for accessing variable-length nibble
     * tracks in TrackStar images.  A default implementation is provided and
     * used for everything but TrackStar.
     */
    virtual int GetNibbleTrackLength(DiskImg::PhysicalFormat physical, int track) const
    {
        if (physical == DiskImg::kPhysicalFormatNib525_6656)
            return kTrackLenNib525;
        else if (physical == DiskImg::kPhysicalFormatNib525_6384)
            return kTrackLenNb2525;
        else {
            assert(false);
            return -1;
        }
    }
    virtual void SetNibbleTrackLength(int track, int length) { /*do nothing*/ }
    virtual int GetNibbleTrackOffset(DiskImg::PhysicalFormat physical, int track) const
    {
        if (physical == DiskImg::kPhysicalFormatNib525_6656 ||
            physical == DiskImg::kPhysicalFormatNib525_6384)
        {
            /* fixed-length tracks */
            return GetNibbleTrackLength(physical, 0) * track;
        } else {
            assert(false);
            return -1;
        }
    }
    // TrackStar images can have more, but otherwise all nibble images have 35
    virtual int GetNibbleNumTracks(void) const
    {
        return kTrackCount525;
    }

private:
    ImageWrapper& operator=(const ImageWrapper&);
    ImageWrapper(const ImageWrapper&);
};


class Wrapper2MG : public ImageWrapper {
public:
    static DIError Test(GenericFD* pGFD, di_off_t wrappedLength);
    virtual DIError Prep(GenericFD* pGFD, di_off_t wrappedLength, bool readOnly,
        di_off_t* pLength, DiskImg::PhysicalFormat* pPhysical,
        DiskImg::SectorOrder* pOrder, short* pDiskVolNum,
        LinearBitmap** ppBadBlockMap, GenericFD** ppNewGFD);
    virtual DIError Create(di_off_t length, DiskImg::PhysicalFormat physical,
        DiskImg::SectorOrder order, short dosVolumeNum, GenericFD* pWrapperGFD,
        di_off_t* pWrappedLength, GenericFD** pDataFD);
    virtual DIError Flush(GenericFD* pWrapperGFD, GenericFD* pDataGFD,
        di_off_t dataLen, di_off_t* pWrappedLen);
    virtual bool HasFastFlush(void) const { return true; }
    //virtual const char* GetComment(void) const { return NULL; }
    // (need to hold TwoImgHeader in the struct, rather than as temp, or
    //  need to copy the comment out into Wrapper2MG storage e.g. StorageName)
};

class WrapperNuFX : public ImageWrapper {
public:
    WrapperNuFX(void) : fpArchive(NULL), fThreadIdx(0), fStorageName(NULL),
        fCompressType(kNuThreadFormatLZW2)
        {}
    virtual ~WrapperNuFX(void) { CloseNuFX(); delete[] fStorageName; }

    static DIError Test(GenericFD* pGFD, di_off_t wrappedLength);
    virtual DIError Prep(GenericFD* pGFD, di_off_t wrappedLength, bool readOnly,
        di_off_t* pLength, DiskImg::PhysicalFormat* pPhysical,
        DiskImg::SectorOrder* pOrder, short* pDiskVolNum,
        LinearBitmap** ppBadBlockMap, GenericFD** ppNewGFD);
    virtual DIError Create(di_off_t length, DiskImg::PhysicalFormat physical,
        DiskImg::SectorOrder order, short dosVolumeNum, GenericFD* pWrapperGFD,
        di_off_t* pWrappedLength, GenericFD** pDataFD);
    virtual DIError Flush(GenericFD* pWrapperGFD, GenericFD* pDataGFD,
        di_off_t dataLen, di_off_t* pWrappedLen);
    virtual bool HasFastFlush(void) const { return false; }

    void SetStorageName(const char* name) {
        delete[] fStorageName;
        if (name != NULL) {
            fStorageName = new char[strlen(name)+1];
            strcpy(fStorageName, name);
        } else
            fStorageName = NULL;
    }
    void SetCompressType(NuThreadFormat format) { fCompressType = format; }

private:
    enum { kDefaultStorageFssep = ':' };
    static NuResult ErrMsgHandler(NuArchive* pArchive, void* vErrorMessage);
    static DIError OpenNuFX(const char* pathName, NuArchive** ppArchive,
        NuThreadIdx* pThreadIdx, long* pLength, bool readOnly);
    DIError GetNuFXDiskImage(NuArchive* pArchive, NuThreadIdx threadIdx,
        long length, char** ppData);
    static char* GenTempPath(const char* path);
    DIError CloseNuFX(void);
    void UNIXTimeToDateTime(const time_t* pWhen, NuDateTime *pDateTime);

    NuArchive*      fpArchive;
    NuThreadIdx     fThreadIdx;
    char*           fStorageName;
    NuThreadFormat  fCompressType;
};

class WrapperDiskCopy42 : public ImageWrapper {
public:
    WrapperDiskCopy42(void) : fStorageName(NULL), fBadChecksum(false)
        {}
    virtual ~WrapperDiskCopy42(void) { delete[] fStorageName; }

    static DIError Test(GenericFD* pGFD, di_off_t wrappedLength);
    virtual DIError Prep(GenericFD* pGFD, di_off_t wrappedLength, bool readOnly,
        di_off_t* pLength, DiskImg::PhysicalFormat* pPhysical,
        DiskImg::SectorOrder* pOrder, short* pDiskVolNum,
        LinearBitmap** ppBadBlockMap, GenericFD** ppNewGFD);
    virtual DIError Create(di_off_t length, DiskImg::PhysicalFormat physical,
        DiskImg::SectorOrder order, short dosVolumeNum, GenericFD* pWrapperGFD,
        di_off_t* pWrappedLength, GenericFD** pDataFD);
    virtual DIError Flush(GenericFD* pWrapperGFD, GenericFD* pDataGFD,
        di_off_t dataLen, di_off_t* pWrappedLen);
    void SetStorageName(const char* name) {
        delete[] fStorageName;
        if (name != NULL) {
            fStorageName = new char[strlen(name)+1];
            strcpy(fStorageName, name);
        } else
            fStorageName = NULL;
    }

    virtual bool HasFastFlush(void) const { return false; }
    virtual bool IsDamaged(void) const { return fBadChecksum; }

private:
    typedef struct DC42Header DC42Header;
    static void DumpHeader(const DC42Header* pHeader);
    void InitHeader(DC42Header* pHeader);
    static int ReadHeader(GenericFD* pGFD, DC42Header* pHeader);
    DIError WriteHeader(GenericFD* pGFD, const DC42Header* pHeader);
    static DIError ComputeChecksum(GenericFD* pGFD,
        unsigned long* pChecksum);

    char*           fStorageName;
    bool            fBadChecksum;
};

class WrapperDDD : public ImageWrapper {
public:
    static DIError Test(GenericFD* pGFD, di_off_t wrappedLength);
    virtual DIError Prep(GenericFD* pGFD, di_off_t wrappedLength, bool readOnly,
        di_off_t* pLength, DiskImg::PhysicalFormat* pPhysical,
        DiskImg::SectorOrder* pOrder, short* pDiskVolNum,
        LinearBitmap** ppBadBlockMap, GenericFD** ppNewGFD);
    virtual DIError Create(di_off_t length, DiskImg::PhysicalFormat physical,
        DiskImg::SectorOrder order, short dosVolumeNum, GenericFD* pWrapperGFD,
        di_off_t* pWrappedLength, GenericFD** pDataFD);
    virtual DIError Flush(GenericFD* pWrapperGFD, GenericFD* pDataGFD,
        di_off_t dataLen, di_off_t* pWrappedLen);
    virtual bool HasFastFlush(void) const { return false; }

    enum {
        kMaxDDDZeroCount = 4,       // 3 observed, 4 suspected
    };

private:
    class BitBuffer;
    enum {
        kNumTracks = 35,
        kNumSectors = 16,
        kSectorSize = 256,
        kTrackLen = kNumSectors * kSectorSize,
    };

    static DIError CheckForRuns(GenericFD* pGFD);
    static DIError Unpack(GenericFD* pGFD, GenericFD** ppNewGFD,
        short* pDiskVolNum);

    static DIError UnpackDisk(GenericFD* pGFD, GenericFD* pNewGFD,
        short* pDiskVolNum);
    static bool UnpackTrack(BitBuffer* pBitBuffer, unsigned char* trackBuf);
    static DIError PackDisk(GenericFD* pSrcGFD, GenericFD* pWrapperGFD,
        short diskVolNum);
    static void PackTrack(const unsigned char* trackBuf, BitBuffer* pBitBuf);
    static void ComputeFreqCounts(const unsigned char* trackBuf,
        unsigned short* freqCounts);
    static void ComputeFavorites(unsigned short* freqCounts,
        unsigned char* favorites);

    short       fDiskVolumeNum;
};

class WrapperSim2eHDV : public ImageWrapper {
public:
    static DIError Test(GenericFD* pGFD, di_off_t wrappedLength);
    virtual DIError Prep(GenericFD* pGFD, di_off_t wrappedLength, bool readOnly,
        di_off_t* pLength, DiskImg::PhysicalFormat* pPhysical,
        DiskImg::SectorOrder* pOrder, short* pDiskVolNum,
        LinearBitmap** ppBadBlockMap, GenericFD** ppNewGFD);
    virtual DIError Create(di_off_t length, DiskImg::PhysicalFormat physical,
        DiskImg::SectorOrder order, short dosVolumeNum, GenericFD* pWrapperGFD,
        di_off_t* pWrappedLength, GenericFD** pDataFD);
    virtual DIError Flush(GenericFD* pWrapperGFD, GenericFD* pDataGFD,
        di_off_t dataLen, di_off_t* pWrappedLen);
    virtual bool HasFastFlush(void) const { return true; }
};

class WrapperTrackStar : public ImageWrapper {
public:
    enum {
        kTrackStarNumTracks = 40,
        kFileTrackStorageLen = 6656,
        kMaxTrackLen = kFileTrackStorageLen - (128+1+2),    // header + footer
        kCommentFieldLen = 0x2e,
    };

    WrapperTrackStar(void) : fStorageName(NULL) {
        memset(&fNibbleTrackInfo, 0, sizeof(fNibbleTrackInfo));
        fNibbleTrackInfo.numTracks = -1;
    }
    virtual ~WrapperTrackStar(void) { delete[] fStorageName; }

    static DIError Test(GenericFD* pGFD, di_off_t wrappedLength);
    virtual DIError Prep(GenericFD* pGFD, di_off_t wrappedLength, bool readOnly,
        di_off_t* pLength, DiskImg::PhysicalFormat* pPhysical,
        DiskImg::SectorOrder* pOrder, short* pDiskVolNum,
        LinearBitmap** ppBadBlockMap, GenericFD** ppNewGFD);
    virtual DIError Create(di_off_t length, DiskImg::PhysicalFormat physical,
        DiskImg::SectorOrder order, short dosVolumeNum, GenericFD* pWrapperGFD,
        di_off_t* pWrappedLength, GenericFD** pDataFD);
    virtual DIError Flush(GenericFD* pWrapperGFD, GenericFD* pDataGFD,
        di_off_t dataLen, di_off_t* pWrappedLen);
    virtual bool HasFastFlush(void) const { return false; }

    virtual void SetStorageName(const char* name)
    {
        delete[] fStorageName;
        if (name != NULL) {
            fStorageName = new char[strlen(name)+1];
            strcpy(fStorageName, name);
        } else
            fStorageName = NULL;
    }

private:
    static DIError VerifyTrack(int track, const unsigned char* trackBuf);
    DIError Unpack(GenericFD* pGFD, GenericFD** ppNewGFD);
    DIError UnpackDisk(GenericFD* pGFD, GenericFD* pNewGFD);

    int     fImageTracks;
    char*   fStorageName;

    /*
     * Data structure for managing nibble images with variable-length tracks.
     */
    typedef struct {
        int     numTracks;      // should be 35 or 40
        int     length[kMaxNibbleTracks525];
        int     offset[kMaxNibbleTracks525];
    } NibbleTrackInfo;
    NibbleTrackInfo fNibbleTrackInfo;   // count and lengths for variable formats

    // nibble images can have variable-length data fields
    virtual int GetNibbleTrackLength(DiskImg::PhysicalFormat physical, int track) const
    {
        assert(physical == DiskImg::kPhysicalFormatNib525_Var);
        assert(fNibbleTrackInfo.numTracks > 0);

        return fNibbleTrackInfo.length[track];
    }
    virtual void SetNibbleTrackLength(int track, int length);
#if 0
    {
        assert(track >= 0);
        assert(length > 0 && length <= kMaxTrackLen);
        assert(track < fNibbleTrackInfo.numTracks);

        fNibbleTrackInfo.length[track] = length;
    }
#endif
    virtual int GetNibbleTrackOffset(DiskImg::PhysicalFormat physical, int track) const
    {
        assert(physical == DiskImg::kPhysicalFormatNib525_Var);
        assert(fNibbleTrackInfo.numTracks > 0);

        return fNibbleTrackInfo.offset[track];
    }
    virtual int GetNibbleNumTracks(void) const
    {
        return kTrackStarNumTracks;
    }
};

class WrapperFDI : public ImageWrapper {
public:
    WrapperFDI(void) {}
    virtual ~WrapperFDI(void) {}

    static DIError Test(GenericFD* pGFD, di_off_t wrappedLength);
    virtual DIError Prep(GenericFD* pGFD, di_off_t wrappedLength, bool readOnly,
        di_off_t* pLength, DiskImg::PhysicalFormat* pPhysical,
        DiskImg::SectorOrder* pOrder, short* pDiskVolNum,
        LinearBitmap** ppBadBlockMap, GenericFD** ppNewGFD);
    virtual DIError Create(di_off_t length, DiskImg::PhysicalFormat physical,
        DiskImg::SectorOrder order, short dosVolumeNum, GenericFD* pWrapperGFD,
        di_off_t* pWrappedLength, GenericFD** pDataFD);
    virtual DIError Flush(GenericFD* pWrapperGFD, GenericFD* pDataGFD,
        di_off_t dataLen, di_off_t* pWrappedLen);
    virtual bool HasFastFlush(void) const { return false; }

    enum {
        kSignatureLen = 27,
        kCreatorLen = 30,
        kCommentLen = 80,
    };

private:
    static const char* kFDIMagic;

    /* what type of disk is this? */
    typedef enum DiskType {
        kDiskType8          = 0,
        kDiskType525        = 1,
        kDiskType35         = 2,
        kDiskType3          = 3
    } DiskType;

    /*
     * Contents of FDI header.
     */
    typedef struct FDIHeader {
        char            signature[kSignatureLen+1];
        char            creator[kCreatorLen+1];
        // CR + LF
        char            comment[kCommentLen+1];
        // MS-DOS EOF
        unsigned short  version;
        unsigned short  lastTrack;
        unsigned char   lastHead;
        unsigned char   type;       // DiskType enum
        unsigned char   rotSpeed;
        unsigned char   flags;
        unsigned char   tpi;
        unsigned char   headWidth;
        unsigned short  reserved;
        // track descriptors follow, at byte 152
    } FDIHeader;

    /*
     * Header for pulse-index streams track.
     */
    typedef struct PulseIndexHeader {
        long        numPulses;
        long        avgStreamLen;
        int         avgStreamCompression;
        long        minStreamLen;
        int         minStreamCompression;
        long        maxStreamLen;
        int         maxStreamCompression;
        long        idxStreamLen;
        int         idxStreamCompression;

        unsigned long*  avgStream;      // 4 bytes/pulse
        unsigned long*  minStream;      // 4 bytes/pulse; optional
        unsigned long*  maxStream;      // 4 bytes/pulse; optional
        unsigned long*  idxStream;      // 2 bytes/pulse; optional?
    } PulseIndexHeader;

    enum {
        kTrackDescrOffset = 152,
        kMaxHeads = 2,
        kMaxHeaderBlockTracks = 180,    // max 90 double-sided cylinders
        kMinHeaderLen = 512,
        kMinVersion = 0x0200,           // v2.0

        kMaxNibbleTracks35 = 80,        // 80 double-sided tracks
        kNibbleBufLen = 10240,          // max seems to be a little under 10K
        kBitBufferSize = kNibbleBufLen + (kNibbleBufLen / 4),

        kMaxSectors35 = 12,             // max #of sectors per track
        //kBytesPerSector35 = 512,      // bytes per sector on 3.5" disk

        kPulseStreamDataOffset = 16,    // start of header to avg stream

        kBitRate525 = 250000,           // 250Kbits/sec
    };

    /* meaning of the two-bit compression format value */
    typedef enum CompressedFormat {
        kCompUncompressed   = 0,
        kCompHuffman        = 1,
    } CompressedFormat;

    /* node in the Huffman tree */
    typedef struct HuffNode {
        unsigned short      val;
        struct HuffNode*    left;
        struct HuffNode*    right;
    } HuffNode;

    /* 
     * Keep a copy of the header around while we work.  None of the formats
     * we're interested in have more than kMaxHeaderBlockTracks tracks in
     * them, so we don't need anything beyond the initial 512-byte header.
     */
    unsigned char fHeaderBuf[kMinHeaderLen];

    static void UnpackHeader(const unsigned char* headerBuf, FDIHeader* hdr);
    static void DumpHeader(const FDIHeader* pHdr);

    DIError Unpack525(GenericFD* pGFD, GenericFD** ppNewGFD, int numCyls,
        int numHeads);
    DIError Unpack35(GenericFD* pGFD, GenericFD** ppNewGFD, int numCyls,
        int numHeads, LinearBitmap** ppBadBlockMap);
    DIError PackDisk(GenericFD* pSrcGFD, GenericFD* pWrapperGFD);

    DIError UnpackDisk525(GenericFD* pGFD, GenericFD* pNewGFD, int numCyls,
        int numHeads);
    DIError UnpackDisk35(GenericFD* pGFD, GenericFD* pNewGFD, int numCyls,
        int numHeads, LinearBitmap* pBadBlockMap);
    void GetTrackInfo(int trk, int* pType, int* pLength256);

    int BitRate35(int trk);
    void FixBadNibbles(unsigned char* nibbleBuf, long nibbleLen);
    bool DecodePulseTrack(const unsigned char* inputBuf, long inputLen,
        int bitRate, unsigned char* nibbleBuf, long* pNibbleLen);
    bool UncompressPulseStream(const unsigned char* inputBuf, long inputLen,
        unsigned long* outputBuf, long numPulses, int format, int bytesPerPulse);
    bool ExpandHuffman(const unsigned char* inputBuf, long inputLen,
        unsigned long* outputBuf, long numPulses);
    const unsigned char* HuffExtractTree(const unsigned char* inputBuf,
        HuffNode* pNode, unsigned char* pBits, unsigned char* pBitMask);
    const unsigned char* HuffExtractValues16(const unsigned char* inputBuf,
        HuffNode* pNode);
    const unsigned char* HuffExtractValues8(const unsigned char* inputBuf,
        HuffNode* pNode);
    void HuffFreeNodes(HuffNode* pNode);
    unsigned long HuffSignExtend16(unsigned long val);
    unsigned long HuffSignExtend8(unsigned long val);
    bool ConvertPulseStreamsToNibbles(PulseIndexHeader* pHdr, int bitRate,
        unsigned char* nibbleBuf, long* pNibbleLen);
    bool ConvertPulsesToBits(const unsigned long* avgStream,
        const unsigned long* minStream, const unsigned long* maxStream,
        const unsigned long* idxStream, int numPulses, int maxIndex,
        int indexOffset, unsigned long totalAvg, int bitRate,
        unsigned char* outputBuf, int* pOutputLen);
    int MyRand(void);
    bool ConvertBitsToNibbles(const unsigned char* bitBuffer, int bitCount,
        unsigned char* nibbleBuf, long* pNibbleLen);


    int     fImageTracks;
    char*   fStorageName;


    /*
     * Data structure for managing nibble images with variable-length tracks.
     */
    typedef struct {
        int     numTracks;      // expect 35 or 40 for 5.25"
        int     length[kMaxNibbleTracks525];
        int     offset[kMaxNibbleTracks525];
    } NibbleTrackInfo;
    NibbleTrackInfo fNibbleTrackInfo;   // count and lengths for variable formats

    // nibble images can have variable-length data fields
    virtual int GetNibbleTrackLength(DiskImg::PhysicalFormat physical, int track) const
    {
        assert(physical == DiskImg::kPhysicalFormatNib525_Var);
        assert(fNibbleTrackInfo.numTracks > 0);

        return fNibbleTrackInfo.length[track];
    }
    virtual void SetNibbleTrackLength(int track, int length);
    virtual int GetNibbleTrackOffset(DiskImg::PhysicalFormat physical, int track) const
    {
        assert(physical == DiskImg::kPhysicalFormatNib525_Var);
        assert(fNibbleTrackInfo.numTracks > 0);

        return fNibbleTrackInfo.offset[track];
    }
    virtual int GetNibbleNumTracks(void) const
    {
        return fNibbleTrackInfo.numTracks;
    }
};


class WrapperUnadornedNibble : public ImageWrapper {
public:
    static DIError Test(GenericFD* pGFD, di_off_t wrappedLength);
    virtual DIError Prep(GenericFD* pGFD, di_off_t wrappedLength, bool readOnly,
        di_off_t* pLength, DiskImg::PhysicalFormat* pPhysical,
        DiskImg::SectorOrder* pOrder, short* pDiskVolNum,
        LinearBitmap** ppBadBlockMap, GenericFD** ppNewGFD);
    virtual DIError Create(di_off_t length, DiskImg::PhysicalFormat physical,
        DiskImg::SectorOrder order, short dosVolumeNum, GenericFD* pWrapperGFD,
        di_off_t* pWrappedLength, GenericFD** pDataFD);
    virtual DIError Flush(GenericFD* pWrapperGFD, GenericFD* pDataGFD,
        di_off_t dataLen, di_off_t* pWrappedLen);
    virtual bool HasFastFlush(void) const { return true; }
};

class WrapperUnadornedSector : public ImageWrapper {
public:
    static DIError Test(GenericFD* pGFD, di_off_t wrappedLength);
    virtual DIError Prep(GenericFD* pGFD, di_off_t wrappedLength, bool readOnly,
        di_off_t* pLength, DiskImg::PhysicalFormat* pPhysical,
        DiskImg::SectorOrder* pOrder, short* pDiskVolNum,
        LinearBitmap** ppBadBlockMap, GenericFD** ppNewGFD);
    virtual DIError Create(di_off_t length, DiskImg::PhysicalFormat physical,
        DiskImg::SectorOrder order, short dosVolumeNum, GenericFD* pWrapperGFD,
        di_off_t* pWrappedLength, GenericFD** pDataFD);
    virtual DIError Flush(GenericFD* pWrapperGFD, GenericFD* pDataGFD,
        di_off_t dataLen, di_off_t* pWrappedLen);
    virtual bool HasFastFlush(void) const { return true; }
};


/*
 * ===========================================================================
 *      Non-FS DiskFSs
 * ===========================================================================
 */

/*
 * A "raw" disk, i.e. no filesystem is known.  Useful as a placeholder
 * for applications that demand a DiskFS object even when the filesystem
 * isn't known.
 */
class DISKIMG_API DiskFSUnknown : public DiskFS {
public:
    DiskFSUnknown(void) : DiskFS() {
        strcpy(fDiskVolumeName, "[Unknown]");
        strcpy(fDiskVolumeID, "Unknown FS");
    }
    virtual ~DiskFSUnknown(void) {}

    virtual DIError Initialize(DiskImg* pImg, InitMode initMode) {
        SetDiskImg(pImg);
        return kDIErrNone;
    }

    virtual const char* GetVolumeName(void) const { return fDiskVolumeName; }
    virtual const char* GetVolumeID(void) const { return fDiskVolumeID; }
    virtual const char* GetBareVolumeName(void) const { return NULL; }
    virtual bool GetReadWriteSupported(void) const { return false; }
    virtual bool GetFSDamaged(void) const { return false; }
    virtual DIError GetFreeSpaceCount(long* pTotalUnits, long* pFreeUnits,
        int* pUnitSize) const
        { return kDIErrNotSupported; }

    // Use this if *something* is known about the filesystem, e.g. the
    // partition type on a MacPart disk.
    void SetVolumeInfo(const char* descr) {
        if (strlen(descr) > kMaxVolumeName)
            return;

        fDiskVolumeName[0] = '[';
        strcpy(fDiskVolumeName+1, descr);
        strcat(fDiskVolumeName, "]");
        strcpy(fDiskVolumeID, "Unknown FS - ");
        strcat(fDiskVolumeID, descr);
    }

private:
    enum { kMaxVolumeName = 64 };

    char    fDiskVolumeName[kMaxVolumeName+3];
    char    fDiskVolumeID[kMaxVolumeName + 20];
};


/*
 * Generic "container" DiskFS class.  Contains some common functions shared
 * among classes that are just containers for other filesystems.  This class
 * is not expected to be instantiated.
 *
 * TODO: create a common OpenSubVolume() function.
 */
class DISKIMG_API DiskFSContainer : public DiskFS {
public:
    DiskFSContainer(void) : DiskFS() {}
    virtual ~DiskFSContainer(void) {}

protected:
    virtual const char* GetDebugName(void) = 0;
    virtual DIError CreatePlaceholder(long startBlock, long numBlocks,
        const char* partName, const char* partType,
        DiskImg** ppNewImg, DiskFS** ppNewFS);
    virtual void SetVolumeUsageMap(void);
};

/*
 * UNIDOS disk, an 800K floppy with two 400K DOS 3.3 volumes on it.
 *
 * The disk itself has no files; instead, it has two embedded sub-volumes.
 */
class DISKIMG_API DiskFSUNIDOS : public DiskFSContainer {
public:
    DiskFSUNIDOS(void) : DiskFSContainer() {}
    virtual ~DiskFSUNIDOS(void) {}

    static DIError TestFS(DiskImg* pImg, DiskImg::SectorOrder* pOrder,
        DiskImg::FSFormat* pFormat, FSLeniency leniency);
    static DIError TestWideFS(DiskImg* pImg, DiskImg::SectorOrder* pOrder,
        DiskImg::FSFormat* pFormat, FSLeniency leniency);

    virtual DIError Initialize(DiskImg* pImg, InitMode initMode) {
        SetDiskImg(pImg);
        return Initialize();
    }

    virtual const char* GetVolumeName(void) const { return "[UNIDOS]"; }
    virtual const char* GetVolumeID(void) const { return "[UNIDOS]"; }
    virtual const char* GetBareVolumeName(void) const { return NULL; }
    virtual bool GetReadWriteSupported(void) const { return false; }
    virtual bool GetFSDamaged(void) const { return false; }
    virtual DIError GetFreeSpaceCount(long* pTotalUnits, long* pFreeUnits,
        int* pUnitSize) const
        { return kDIErrNotSupported; }

private:
    virtual const char* GetDebugName(void) { return "UNIDOS"; }
    DIError Initialize(void);
    DIError OpenSubVolume(int idx);
};

/*
 * OzDOS disk, an 800K floppy with two 400K DOS 3.3 volumes on it.  They
 * put the files for disk 1 in the odd sectors and the files for disk 2
 * in the even sectors (the top and bottom halves of a 512-byte block).
 *
 * The disk itself has no files; instead, it has two embedded sub-volumes.
 * Because of the funky layout, we have to use the "sector pairing" feature
 * of DiskImg to treat this like a DOS 3.3 disk.
 */
class DISKIMG_API DiskFSOzDOS : public DiskFSContainer {
public:
    DiskFSOzDOS(void) : DiskFSContainer() {}
    virtual ~DiskFSOzDOS(void) {}

    static DIError TestFS(DiskImg* pImg, DiskImg::SectorOrder* pOrder,
        DiskImg::FSFormat* pFormat, FSLeniency leniency);

    virtual DIError Initialize(DiskImg* pImg, InitMode initMode) {
        SetDiskImg(pImg);
        return Initialize();
    }

    virtual const char* GetVolumeName(void) const { return "[OzDOS]"; }
    virtual const char* GetVolumeID(void) const { return "[OzDOS]"; }
    virtual const char* GetBareVolumeName(void) const { return NULL; }
    virtual bool GetReadWriteSupported(void) const { return false; }
    virtual bool GetFSDamaged(void) const { return false; }
    virtual DIError GetFreeSpaceCount(long* pTotalUnits, long* pFreeUnits,
        int* pUnitSize) const
        { return kDIErrNotSupported; }

private:
    virtual const char* GetDebugName(void) { return "OzDOS"; }
    DIError Initialize(void);
    DIError OpenSubVolume(int idx);
};

/*
 * CFFA volume.  A potentially very large volume with multiple partitions.
 *
 * This DiskFS is just a container that describes the position and sizes
 * of the sub-volumes.
 */
class DISKIMG_API DiskFSCFFA : public DiskFSContainer {
public:
    DiskFSCFFA(void) : DiskFSContainer() {}
    virtual ~DiskFSCFFA(void) {}

    static DIError TestFS(DiskImg* pImg, DiskImg::SectorOrder* pOrder,
        DiskImg::FSFormat* pFormat, FSLeniency leniency);

    virtual DIError Initialize(DiskImg* pImg, InitMode initMode) {
        SetDiskImg(pImg);
        return Initialize();
    }

    virtual const char* GetVolumeName(void) const { return "[CFFA]"; }
    virtual const char* GetVolumeID(void) const { return "[CFFA]"; }
    virtual const char* GetBareVolumeName(void) const { return NULL; }
    virtual bool GetReadWriteSupported(void) const { return false; }
    virtual bool GetFSDamaged(void) const { return false; }
    virtual DIError GetFreeSpaceCount(long* pTotalUnits, long* pFreeUnits,
        int* pUnitSize) const
        { return kDIErrNotSupported; }

private:
    virtual const char* GetDebugName(void) { return "CFFA"; }

    static DIError TestImage(DiskImg* pImg, DiskImg::SectorOrder imageOrder,
        DiskImg::FSFormat* pFormatFound);
    static DIError OpenSubVolume(DiskImg* pImg, long startBlock,
        long numBlocks, bool scanOnly, DiskImg** ppNewImg, DiskFS** ppNewFS);
    DIError Initialize(void);
    DIError FindSubVolumes(void);
    DIError AddVolumeSeries(int start, int count, long blocksPerVolume,
        long& startBlock, long& totalBlocksLeft);

    enum {
        kMinInterestingBlocks = 65536 + 1024,       // less than this, ignore
        kEarlyVolExpectedSize = 65536,              // 32MB in 512-byte blocks
        kOneGB = 1024*1024*(1024/512),              // 1GB in 512-byte blocks
    };
};


/*
 * Macintosh-style partitioned disk image.
 *
 * This DiskFS is just a container that describes the position and sizes
 * of the sub-volumes.
 */
class DISKIMG_API DiskFSMacPart : public DiskFSContainer {
public:
    DiskFSMacPart(void) : DiskFSContainer() {}
    virtual ~DiskFSMacPart(void) {}

    static DIError TestFS(DiskImg* pImg, DiskImg::SectorOrder* pOrder,
        DiskImg::FSFormat* pFormat, FSLeniency leniency);

    virtual DIError Initialize(DiskImg* pImg, InitMode initMode) {
        SetDiskImg(pImg);
        return Initialize();
    }

    virtual const char* GetVolumeName(void) const { return "[MacPartition]"; }
    virtual const char* GetVolumeID(void) const { return "[MacPartition]"; }
    virtual const char* GetBareVolumeName(void) const { return NULL; }
    virtual bool GetReadWriteSupported(void) const { return false; }
    virtual bool GetFSDamaged(void) const { return false; }
    virtual DIError GetFreeSpaceCount(long* pTotalUnits, long* pFreeUnits,
        int* pUnitSize) const
        { return kDIErrNotSupported; }

private:
    virtual const char* GetDebugName(void) { return "MacPart"; }

    struct PartitionMap;            // fwd
    struct DriverDescriptorRecord;  // fwd
    static void UnpackDDR(const unsigned char* buf,
        DriverDescriptorRecord* pDDR);
    static void DumpDDR(const DriverDescriptorRecord* pDDR);
    static void UnpackPartitionMap(const unsigned char* buf,
        PartitionMap* pMap);
    static void DumpPartitionMap(long block, const PartitionMap* pMap);

    static DIError TestImage(DiskImg* pImg, DiskImg::SectorOrder imageOrder);
    DIError OpenSubVolume(const PartitionMap* pMap);
    DIError Initialize(void);
    DIError FindSubVolumes(void);

    enum {
        kMinInterestingBlocks = 2048,       // less than this, ignore
        kDDRSignature = 0x4552,             // 'ER'
        kPartitionSignature = 0x504d,       // 'PM'
    };
};


/*
 * Partitioning for Joachim Lange's MicroDrive card.
 *
 * This DiskFS is just a container that describes the position and sizes
 * of the sub-volumes.
 */
class DISKIMG_API DiskFSMicroDrive : public DiskFSContainer {
public:
    DiskFSMicroDrive(void) : DiskFSContainer() {}
    virtual ~DiskFSMicroDrive(void) {}

    static DIError TestFS(DiskImg* pImg, DiskImg::SectorOrder* pOrder,
        DiskImg::FSFormat* pFormat, FSLeniency leniency);

    virtual DIError Initialize(DiskImg* pImg, InitMode initMode) {
        SetDiskImg(pImg);
        return Initialize();
    }

    virtual const char* GetVolumeName(void) const { return "[MicroDrive]"; }
    virtual const char* GetVolumeID(void) const { return "[MicroDrive]"; }
    virtual const char* GetBareVolumeName(void) const { return NULL; }
    virtual bool GetReadWriteSupported(void) const { return false; }
    virtual bool GetFSDamaged(void) const { return false; }
    virtual DIError GetFreeSpaceCount(long* pTotalUnits, long* pFreeUnits,
        int* pUnitSize) const
        { return kDIErrNotSupported; }

private:
    virtual const char* GetDebugName(void) { return "MicroDrive"; }

    struct PartitionMap;            // fwd
    static void UnpackPartitionMap(const unsigned char* buf,
        PartitionMap* pMap);
    static void DumpPartitionMap(const PartitionMap* pMap);

    static DIError TestImage(DiskImg* pImg, DiskImg::SectorOrder imageOrder);
    DIError OpenSubVolume(long startBlock, long numBlocks);
    DIError OpenVol(int idx, long startBlock, long numBlocks);
    DIError Initialize(void);
    DIError FindSubVolumes(void);

    enum {
        kMinInterestingBlocks = 2048,       // less than this, ignore
        kPartitionSignature = 0xccca,       // 'JL' in little-endian high-ASCII
    };
};


/*
 * Partitioning for Parsons Engineering FocusDrive card.
 *
 * This DiskFS is just a container that describes the position and sizes
 * of the sub-volumes.
 */
class DISKIMG_API DiskFSFocusDrive : public DiskFSContainer {
public:
    DiskFSFocusDrive(void) : DiskFSContainer() {}
    virtual ~DiskFSFocusDrive(void) {}

    static DIError TestFS(DiskImg* pImg, DiskImg::SectorOrder* pOrder,
        DiskImg::FSFormat* pFormat, FSLeniency leniency);

    virtual DIError Initialize(DiskImg* pImg, InitMode initMode) {
        SetDiskImg(pImg);
        return Initialize();
    }

    virtual const char* GetVolumeName(void) const { return "[FocusDrive]"; }
    virtual const char* GetVolumeID(void) const { return "[FocusDrive]"; }
    virtual const char* GetBareVolumeName(void) const { return NULL; }
    virtual bool GetReadWriteSupported(void) const { return false; }
    virtual bool GetFSDamaged(void) const { return false; }
    virtual DIError GetFreeSpaceCount(long* pTotalUnits, long* pFreeUnits,
        int* pUnitSize) const
        { return kDIErrNotSupported; }

private:
    virtual const char* GetDebugName(void) { return "FocusDrive"; }

    struct PartitionMap;            // fwd
    static void UnpackPartitionMap(const unsigned char* buf,
        const unsigned char* nameBuf, PartitionMap* pMap);
    static void DumpPartitionMap(const PartitionMap* pMap);

    static DIError TestImage(DiskImg* pImg, DiskImg::SectorOrder imageOrder);
    DIError OpenSubVolume(long startBlock, long numBlocks,
        const char* name);
    DIError OpenVol(int idx, long startBlock, long numBlocks,
        const char* name);
    DIError Initialize(void);
    DIError FindSubVolumes(void);

    enum {
        kMinInterestingBlocks = 2048,       // less than this, ignore
    };
};


/*
 * ===========================================================================
 *      DOS 3.2/3.3
 * ===========================================================================
 */

class A2FileDOS;

/*
 * DOS 3.2/3.3 disk.
 */
class DISKIMG_API DiskFSDOS33 : public DiskFS {
public:
    DiskFSDOS33(void) : DiskFS() {
        fVTOCLoaded = false;
        fDiskIsGood = false;
    }
    virtual ~DiskFSDOS33(void) {}

    static DIError TestFS(DiskImg* pImg, DiskImg::SectorOrder* pOrder,
        DiskImg::FSFormat* pFormat, FSLeniency leniency);

    virtual DIError Initialize(DiskImg* pImg, InitMode initMode) {
        SetDiskImg(pImg);
        return Initialize(initMode);
    }
    virtual DIError Format(DiskImg* pDiskImg, const char* volName);

    virtual const char* GetVolumeName(void) const { return fDiskVolumeName; }
    virtual const char* GetVolumeID(void) const { return fDiskVolumeID; }
    virtual const char* GetBareVolumeName(void) const {
        // this is fragile -- skip over the "DOS" part, return 3 digits
        assert(strlen(fDiskVolumeName) > 3);
        return fDiskVolumeName+3;
    }
    virtual bool GetReadWriteSupported(void) const { return true; }
    virtual bool GetFSDamaged(void) const { return !fDiskIsGood; }
    virtual DIError GetFreeSpaceCount(long* pTotalUnits, long* pFreeUnits,
        int* pUnitSize) const;
    virtual DIError NormalizePath(const char* path, char fssep,
        char* normalizedBuf, int* pNormalizedBufLen);
    virtual DIError CreateFile(const CreateParms* pParms, A2File** ppNewFile);
    virtual DIError DeleteFile(A2File* pFile);
    virtual DIError RenameFile(A2File* pFile, const char* newName);
    virtual DIError SetFileInfo(A2File* pFile, long fileType, long auxType,
        long accessFlags);
    virtual DIError RenameVolume(const char* newName);

    /*
     * Unique to DOS 3.3 disks.
     */
    int GetDiskVolumeNum(void) const { return fDiskVolumeNum; }
    void SetDiskVolumeNum(int val);

    static bool IsValidFileName(const char* name);
    static bool IsValidVolumeName(const char* name);

    // utility function
    static void LowerASCII(unsigned char* buf, long len);
    static void ReplaceFssep(char* str, char replacement);

    enum {
        kMinTracks = 17,            // need to put the catalog track here
        kMaxTracks = 50,
        kMaxCatalogSectors = 64,    // two tracks on a 32-sector disk
    };

    /* a T/S pair */
    typedef struct TrackSector {
        char    track;
        char    sector;
    } TrackSector;

    friend class A2FDDOS;   // for Write

private:
    DIError Initialize(InitMode initMode);
    DIError ReadVTOC(void);
    void UpdateVolumeNum(void);
    void DumpVTOC(void);
    void SetSectorUsage(long track, long sector,
        VolumeUsage::ChunkPurpose purpose);
    void FixVolumeUsageMap(void);
    DIError ReadCatalog(void);
    DIError ProcessCatalogSector(int catTrack, int catSect,
        const unsigned char* sctBuf);
    DIError GetFileLengths(void);
    DIError ComputeLength(A2FileDOS* pFile, const TrackSector* tsList,
        int tsCount);
    DIError TrimLastSectorUp(A2FileDOS* pFile, TrackSector lastTS);
    void MarkFileUsage(A2FileDOS* pFile, TrackSector* tsList, int tsCount,
        TrackSector* indexList, int indexCount);
    //DIError TrimLastSectorDown(A2FileDOS* pFile, unsigned short* tsBuf,
    //  int maxZeroCount);
    void DoNormalizePath(const char* name, char fssep, char* outBuf);
    DIError MakeFileNameUnique(char* fileName);
    DIError GetFreeCatalogEntry(TrackSector* pCatSect, int* pCatEntry,
        unsigned char* sctBuf, A2FileDOS** ppPrevEntry);
    void CreateDirEntry(unsigned char* sctBuf, int catEntry,
        const char* fileName, TrackSector* pTSSect, unsigned char fileType,
        int access);
    void FreeTrackSectors(TrackSector* pList, int count);

    bool CheckDiskIsGood(void);

    DIError WriteDOSTracks(int sectPerTrack);

    DIError ScanVolBitmap(void);
    DIError LoadVolBitmap(void);
    DIError SaveVolBitmap(void);
    void FreeVolBitmap(void);
    DIError AllocSector(TrackSector* pTS);
    DIError CreateEmptyBlockMap(bool withDOS);
    bool GetSectorUseEntry(long track, int sector) const;
    void SetSectorUseEntry(long track, int sector, bool inUse);
    inline unsigned long GetVTOCEntry(const unsigned char* pVTOC,
        long track) const;

    // Largest interesting volume is 400K (50 tracks, 32 sectors), but
    // we may be looking at it in 16-sector mode, so max tracks is 100.
    enum {
        kMaxInterestingTracks = 100,
        kSectorSize = 256,
        kDefaultVolumeNum = 254,
        kMaxExtensionLen = 4,   // used when normalizing; ".gif" is 4
    };

    // DOS track images, for initializing disk images
    static const unsigned char gDOS33Tracks[];
    static const unsigned char gDOS32Tracks[];

    /* some fields from the VTOC */
    int     fFirstCatTrack;
    int     fFirstCatSector;
    int     fVTOCVolumeNumber;
    int     fVTOCNumTracks;
    int     fVTOCNumSectors;

    /* private data */
    int     fDiskVolumeNum;         // usually 254
    char    fDiskVolumeName[7];     // "DOS" + num, e.g. "DOS001", "DOS254"
    char    fDiskVolumeID[32];      // sizeof "DOS 3.3 Volume " +3 +1
    unsigned char   fVTOC[kSectorSize];
    bool    fVTOCLoaded;

    /*
     * There are some things we need to be careful of when reading the
     * catalog track, like bad links and infinite loops.  By storing a list
     * of known good catalog sectors, we only have to handle that stuff once.
     * The catalog doesn't grow or shrink, so this never needs to be updated.
     */
    TrackSector fCatalogSectors[kMaxCatalogSectors];

    bool    fDiskIsGood;
};

/*
 * File descriptor for an open DOS file.
 */
class DISKIMG_API A2FDDOS : public A2FileDescr {
public:
    A2FDDOS(A2File* pFile) : A2FileDescr(pFile) {
        fTSList = NULL;
        fIndexList = NULL;
        fOffset = 0;
        fModified = false;
    }
    virtual ~A2FDDOS(void) {
        delete[] fTSList;
        delete[] fIndexList;
        //fTSList = fIndexList = NULL;
    }

    //typedef DiskFSDOS33::TrackSector TrackSector;

    friend class A2FileDOS;

    virtual DIError Read(void* buf, size_t len, size_t* pActual = NULL);
    virtual DIError Write(const void* buf, size_t len, size_t* pActual = NULL);
    virtual DIError Seek(di_off_t offset, DIWhence whence);
    virtual di_off_t Tell(void);
    virtual DIError Close(void);

    virtual long GetSectorCount(void) const;
    virtual long GetBlockCount(void) const;
    virtual DIError GetStorage(long sectorIdx, long* pTrack, long* pSector) const;
    virtual DIError GetStorage(long blockIdx, long* pBlock) const;

private:
    typedef DiskFSDOS33::TrackSector TrackSector;

    TrackSector*    fTSList;            // T/S entries for data sectors
    int             fTSCount;
    TrackSector*    fIndexList;         // T/S entries for T/S list sectors
    int             fIndexCount;
    di_off_t        fOffset;            // current position in file

    di_off_t        fOpenEOF;           // how big the file currently is
    long            fOpenSectorsUsed;   // how many sectors it occupies
    bool            fModified;          // if modified, update stuff on Close

    void DumpTSList(void) const;
};

/*
 * Holds DOS files.  Works for DOS33, DOS32, and "wide" DOS implementations.
 *
 * Catalog contents are public so anyone who wants gory details of DOS 3.3
 * stuff can poke at whatever they want.  Anybody else can use the virtual
 * interfaces to get standardized answers for things like file type.
 *
 * The embedded address and length fields found in Applesoft, Integer, and
 * Binary files are quietly skipped over with the fDataOffset field when
 * files are read.
 *
 * THOUGHT: have "get filename" and "get raw filename" interfaces?  There
 * are no directories, so maybe we don't care about "raw pathname"??  Might
 * be better to always return the "raw" value and let the caller deal with
 * things like high ASCII.
 */
class DISKIMG_API A2FileDOS : public A2File {
public:
    A2FileDOS(DiskFS* pDiskFS);
    virtual ~A2FileDOS(void);

    // assorted constants
    enum {
        kMaxFileName = 30,
    };
    typedef enum {
        kTypeUnknown        = -1,
        kTypeText           = 0x00,     // 'T'
        kTypeInteger        = 0x01,     // 'I'
        kTypeApplesoft      = 0x02,     // 'A' 
        kTypeBinary         = 0x04,     // 'B'
        kTypeS              = 0x08,     // 'S'
        kTypeReloc          = 0x10,     // 'R'
        kTypeA              = 0x20,     // 'A'
        kTypeB              = 0x40,     // 'B'

        kTypeLocked         = 0x80      // bitwise OR with previous values
    } FileType;

    /*
     * Implementations of standard interfaces.
     */
    virtual const char* GetFileName(void) const { return fFileName; }
    virtual const char* GetPathName(void) const { return fFileName; }
    virtual char GetFssep(void) const { return '\0'; }
    virtual long GetFileType(void) const;
    virtual long GetAuxType(void) const { return fAuxType; }
    virtual long GetAccess(void) const;
    virtual time_t GetCreateWhen(void) const { return 0; }
    virtual time_t GetModWhen(void) const { return 0; }
    virtual di_off_t GetDataLength(void) const { return fLength; }
    virtual di_off_t GetDataSparseLength(void) const { return fSparseLength; }
    virtual di_off_t GetRsrcLength(void) const { return -1; }
    virtual di_off_t GetRsrcSparseLength(void) const { return -1; }

    virtual DIError Open(A2FileDescr** ppOpenFile, bool readOnly,
        bool rsrcFork = false);
    virtual void CloseDescr(A2FileDescr* pOpenFile) {
        assert(pOpenFile == fpOpenFile);
        delete fpOpenFile;
        fpOpenFile = NULL;
    }
    virtual bool IsFileOpen(void) const { return fpOpenFile != NULL; }

    void Dump(void) const;

    typedef DiskFSDOS33::TrackSector TrackSector;

    /*
     * Contents of directory entry.
     *
     * We don't hold deleted or unused entries, so fTSListTrack is always
     * valid.
     */
    short       fTSListTrack;       // (could use TrackSector here)
    short       fTSListSector;
    unsigned short  fLengthInSectors;
    bool        fLocked;
    char        fFileName[kMaxFileName+1];  // "fixed" version
    FileType    fFileType;

    TrackSector fCatTS;         // track/sector for our catalog entry
    int         fCatEntryNum;   // entry number within cat sector

    // these are computed or determined from the file contents
    unsigned short  fAuxType;           // addr for bin, etc.
    short           fDataOffset;        // for 'A'/'B'/'I' with embedded len
    di_off_t        fLength;            // file length, in bytes
    di_off_t        fSparseLength;      // file length, factoring sparse out

    void FixFilename(void);

    DIError LoadTSList(TrackSector** pTSList, int* pTSCount,
        TrackSector** pIndexList = NULL, int* pIndexCount = NULL);
    static FileType ConvertFileType(long prodosType, di_off_t fileLen);
    static bool IsValidType(long prodosType);
    static void MakeDOSName(char* buf, const char* name);
    static void TrimTrailingSpaces(char* filename);

private:
    DIError ExtractTSPairs(const unsigned char* sctBuf, TrackSector* tsList,
        int* pLastNonZero);

    A2FDDOS*        fpOpenFile;
};


/*
 * ===========================================================================
 *      ProDOS
 * ===========================================================================
 */

class A2FileProDOS;

/*
 * ProDOS disk.
 *
 * THOUGHT: it would be undesirable for the CiderPress UI, but it would
 * make things somewhat easier internally if we treated the volume dir
 * like a subdirectory under which everything else sits, instead of special-
 * casing it like we do.  This is awkward because volume dirs have names
 * under ProDOS, giving every pathname an extra component that they don't
 * really need.  We can never treat the volume dir purely as a subdir,
 * because it can't expand beyond 51 files, but the storage_type in the
 * header is sufficient to identify it as such (assuming the disk isn't
 * broken).  Certain operations, such as changing the file type or aux type,
 * simply aren't possible on a volume dir, and deleting a volume dir doesn't
 * make sense.  So in some respects we simply trade one kind of special-case
 * behavior for another.
 */
class DISKIMG_API DiskFSProDOS : public DiskFS {
public:
    DiskFSProDOS(void) : fBitMapPointer(0), fTotalBlocks(0), fBlockUseMap(NULL)
        {}
    virtual ~DiskFSProDOS(void) {
        if (fBlockUseMap != NULL) {
            assert(false);  // unexpected
            delete[] fBlockUseMap;
        }
    }

    static DIError TestFS(DiskImg* pImg, DiskImg::SectorOrder* pOrder,
        DiskImg::FSFormat* pFormat, FSLeniency leniency);

    virtual DIError Initialize(DiskImg* pImg, InitMode initMode) {
        SetDiskImg(pImg);
        return Initialize(initMode);
    }
    virtual DIError Format(DiskImg* pDiskImg, const char* volName);
    virtual DIError NormalizePath(const char* path, char fssep,
        char* normalizedBuf, int* pNormalizedBufLen);
    virtual DIError CreateFile(const CreateParms* pParms, A2File** ppNewFile);
    virtual DIError DeleteFile(A2File* pFile);
    virtual DIError RenameFile(A2File* pFile, const char* newName);
    virtual DIError SetFileInfo(A2File* pFile, long fileType, long auxType,
        long accessFlags);
    virtual DIError RenameVolume(const char* newName);

    // assorted constants
    enum {
        kMaxVolumeName = 15,
    };
    typedef unsigned long ProDate;

    virtual const char* GetVolumeName(void) const { return fVolumeName; }
    virtual const char* GetVolumeID(void) const { return fVolumeID; }
    virtual const char* GetBareVolumeName(void) const { return fVolumeName; }
    virtual bool GetReadWriteSupported(void) const { return true; }
    virtual bool GetFSDamaged(void) const { return !fDiskIsGood; }
    virtual long GetFSNumBlocks(void) const { return fTotalBlocks; }
    virtual DIError GetFreeSpaceCount(long* pTotalUnits, long* pFreeUnits,
        int* pUnitSize) const;

    //A2FileProDOS* GetVolDir(void) const { return fpVolDir; }

    static bool IsValidFileName(const char* name);
    static bool IsValidVolumeName(const char* name);
    static unsigned short GenerateLowerCaseBits(const char* upperName,
        const char* lowerName, bool forAppleWorks);
    static void GenerateLowerCaseName(const char* upperName,
        char* lowerNameNoTerm, unsigned short lcFlags, bool fromAppleWorks);

    friend class A2FDProDOS;

private:
    struct DirHeader;

    enum { kMaxExtensionLen = 4 };  // used when normalizing; ".gif" is 4

    DIError Initialize(InitMode initMode);
    DIError LoadVolHeader(void);
    void SetVolumeID(void);
    void DumpVolHeader(void);
    DIError ScanVolBitmap(void);
    DIError LoadVolBitmap(void);
    DIError SaveVolBitmap(void);
    void FreeVolBitmap(void);
    long AllocBlock(void);
    int GetNumBitmapBlocks(void) const {
        /* use fTotalBlocks rather than GetNumBlocks() */
        assert(fTotalBlocks > 0);
        const int kBitsPerBlock = 512 * 8;
        int numBlocks = (fTotalBlocks + kBitsPerBlock-1) / kBitsPerBlock;
        return numBlocks;
    }
    DIError CreateEmptyBlockMap(void);
    bool GetBlockUseEntry(long block) const;
    void SetBlockUseEntry(long block, bool inUse);
    bool ScanForExtraEntries(void) const;

    void SetBlockUsage(long block, VolumeUsage::ChunkPurpose purpose);
    DIError GetDirHeader(const unsigned char* blkBuf, DirHeader* pHeader);
    DIError RecursiveDirAdd(A2File* pParent, unsigned short dirBlock,
        const char* basePath, int depth);
    DIError SlurpEntries(A2File* pParent, const DirHeader* pHeader,
        const unsigned char* blkBuf, bool skipFirst, int* pCount,
        const char* basePath, unsigned short thisBlock, int depth);
    DIError ReadExtendedInfo(A2FileProDOS* pFile);
    DIError ScanFileUsage(void);
    void ScanBlockList(long blockCount, unsigned short* blockList,
        long indexCount, unsigned short* indexList, long* pSparseCount);
    DIError ScanForSubVolumes(void);
    DIError FindSubVolume(long blockStart, long blockCount,
        DiskImg** ppDiskImg, DiskFS** ppDiskFS);
    void MarkSubVolumeBlocks(long block, long count);

    A2File* FindFileByKeyBlock(A2File* pStart, unsigned short keyBlock);
    DIError AllocInitialFileStorage(const CreateParms* pParms,
        const char* upperName, unsigned short dirBlock, int dirEntrySlot,
        long* pKeyBlock, int* pBlocksUsed, int* pNewEOF);
    DIError WriteBootBlocks(void);
    DIError DoNormalizePath(const char* path, char fssep,
        char** pNormalizedPath);
    void UpperCaseName(char* upperName, const char* name);
    bool CheckDiskIsGood(void);
    DIError AllocDirEntry(A2FileDescr* pOpenSubdir, unsigned char** ppDir,
        long* pDirLen, unsigned char** ppDirEntry, unsigned short* pDirKeyBlock,
        int* pDirEntrySlot, unsigned short* pDirBlock);
    unsigned char* GetPrevDirEntry(unsigned char* buf, unsigned char* ptr);
    DIError MakeFileNameUnique(const unsigned char* dirBuf, long dirLen,
        char* fileName);
    bool NameExistsInDir(const unsigned char* dirBuf, long dirLen,
        const char* fileName);

    DIError FreeBlocks(long blockCount, unsigned short* blockList);
    DIError RegeneratePathName(A2FileProDOS* pFile);

    /* some items from the volume header */
    char            fVolumeName[kMaxVolumeName+1];
    char            fVolumeID[kMaxVolumeName + 16]; // add "ProDOS /"
    unsigned char   fAccess;
    ProDate         fCreateWhen;
    ProDate         fModWhen;
    unsigned short  fBitMapPointer;
    unsigned short  fTotalBlocks;
    //unsigned short    fPrevBlock;
    //unsigned short    fNextBlock;
    //unsigned char fVersion;
    //unsigned char fMinVersion;
    //unsigned char fEntryLength;
    //unsigned char fEntriesPerBlock;
    unsigned short  fVolDirFileCount;

//  A2FileProDOS*   fpVolDir;       // a "fake" file entry for the volume dir

    /*
     * This is a working copy of the block use map from blocks 6+.  It should
     * be loaded when we're about to modify files on the disk and freed
     * immediately afterward.  The goal is to facilitate speedy updates to the
     * bitmap without creating problems if the application decides to modify
     * one of the bitmap blocks directly (e.g. with the disk sector editor).
     * It should never be held across calls.
     */
    unsigned char*  fBlockUseMap;

    /*
     * Set this if the disk is "perfect".  If it's not, we disallow write
     * access for safety reasons.
     */
    bool            fDiskIsGood;

    /* set if something fixes damage so CheckDiskIsGood can't see it */
    bool            fEarlyDamage;
};

/*
 * File descriptor for an open ProDOS file.
 *
 * This only represents one fork.
 */
class DISKIMG_API A2FDProDOS : public A2FileDescr {
public:
    A2FDProDOS(A2File* pFile) : A2FileDescr(pFile), fModified(false),
        fBlockList(NULL), fOffset(0)
    {}
    virtual ~A2FDProDOS(void) {
        delete[] fBlockList;
        fBlockList = NULL;
    }

    friend class A2FileProDOS;

    virtual DIError Read(void* buf, size_t len, size_t* pActual = NULL);
    virtual DIError Write(const void* buf, size_t len, size_t* pActual = NULL);
    virtual DIError Seek(di_off_t offset, DIWhence whence);
    virtual di_off_t Tell(void);
    virtual DIError Close(void);

    virtual long GetSectorCount(void) const;
    virtual long GetBlockCount(void) const;
    virtual DIError GetStorage(long sectorIdx, long* pTrack, long* pSector) const;
    virtual DIError GetStorage(long blockIdx, long* pBlock) const;

    void DumpBlockList(void) const;

private:
    bool IsEmptyBlock(const unsigned char* blk);
    DIError WriteDirectory(const void* buf, size_t len, size_t* pActual);

    /* state for open files */
    bool            fModified;
    long            fBlockCount;
    unsigned short* fBlockList;
    di_off_t        fOpenEOF;           // current EOF
    unsigned short  fOpenBlocksUsed;    // #of block used by open piece
    int             fOpenStorageType;
    bool            fOpenRsrcFork;      // is this the resource fork?
    di_off_t        fOffset;            // current file offset
};

/*
 * Holds a ProDOS file.
 */
class DISKIMG_API A2FileProDOS : public A2File {
public:
    A2FileProDOS(DiskFS* pDiskFS) : A2File(pDiskFS) {
        fPathName = NULL;
        fSparseDataEof = fSparseRsrcEof = -1;
        fpOpenFile = NULL;
        fParentDirBlock = 0;
        fParentDirIdx = -1;
        fpParent = NULL;
    }
    virtual ~A2FileProDOS(void) {
        delete fpOpenFile;
        delete[] fPathName;
    }

    typedef DiskFSProDOS::ProDate ProDate;

    /* assorted constants */
    enum {
        kMaxFileName = 15,
        kFssep = ':',
        kInvalidBlockNum = 1,       // boot block, can't be in file
        kMaxBlocksPerIndex = 256,
    };
    /* ProDOS access permissions */
    enum {
        kAccessRead         = 0x01,
        kAccessWrite        = 0x02,
        kAccessInvisible    = 0x04,
        kAccessBackup       = 0x20,
        kAccessRename       = 0x40,
        kAccessDelete       = 0x80
    };
    /* contents of a directory entry */
    typedef struct DirEntry {
        int             storageType;
        char            fileName[kMaxFileName+1];   // shows lower case
        unsigned char   fileType;
        unsigned short  keyPointer;
        unsigned short  blocksUsed;
        unsigned long   eof;
        ProDate         createWhen;
        unsigned char   version;
        unsigned char   minVersion;
        unsigned char   access;
        unsigned short  auxType;
        ProDate         modWhen;
        unsigned short  headerPointer;
    } DirEntry;
    typedef struct ExtendedInfo {
        unsigned char   storageType;
        unsigned short  keyBlock;
        unsigned short  blocksUsed;
        unsigned long   eof;
    } ExtendedInfo;
    typedef enum StorageType {
        kStorageDeleted         = 0,      /* indicates deleted file */
        kStorageSeedling        = 1,      /* <= 512 bytes */
        kStorageSapling         = 2,      /* < 128KB */
        kStorageTree            = 3,      /* < 16MB */
        kStoragePascalVolume    = 4,      /* see ProDOS technote 25 */
        kStorageExtended        = 5,      /* forked */
        kStorageDirectory       = 13,
        kStorageSubdirHeader    = 14,
        kStorageVolumeDirHeader = 15,
    } StorageType;

    static bool IsRegularFile(int type) {
        return (type == kStorageSeedling || type == kStorageSapling ||
                type == kStorageTree);
    }

    /*
     * Implementations of standard interfaces.
     */
    virtual const char* GetFileName(void) const { return fDirEntry.fileName; }
    virtual const char* GetPathName(void) const { return fPathName; }
    virtual char GetFssep(void) const { return kFssep; }
    virtual long GetFileType(void) const { return fDirEntry.fileType; }
    virtual long GetAuxType(void) const { return fDirEntry.auxType; }
    virtual long GetAccess(void) const { return fDirEntry.access; }
    virtual time_t GetCreateWhen(void) const;
    virtual time_t GetModWhen(void) const;
    virtual di_off_t GetDataLength(void) const {
        if (GetQuality() == kQualityDamaged)
            return 0;
        if (fDirEntry.storageType == kStorageExtended)
            return fExtData.eof;
        else
            return fDirEntry.eof;
    }
    virtual di_off_t GetRsrcLength(void) const {
        if (fDirEntry.storageType == kStorageExtended) {
            if (GetQuality() == kQualityDamaged)
                return 0;
            else
                return fExtRsrc.eof;
        } else
            return -1;
    }
    virtual di_off_t GetDataSparseLength(void) const {
        if (GetQuality() == kQualityDamaged)
            return 0;
        else
            return fSparseDataEof;
    }
    virtual di_off_t GetRsrcSparseLength(void) const {
        if (GetQuality() == kQualityDamaged)
            return 0;
        else
            return fSparseRsrcEof;
    }
    virtual bool IsDirectory(void) const {
        return (fDirEntry.storageType == kStorageDirectory ||
                fDirEntry.storageType == kStorageVolumeDirHeader);
    }
    virtual bool IsVolumeDirectory(void) const {
        return (fDirEntry.storageType == kStorageVolumeDirHeader);
    }

    virtual DIError Open(A2FileDescr** ppOpenFile, bool readOnly,
        bool rsrcFork = false);
    virtual void CloseDescr(A2FileDescr* pOpenFile) {
        assert(pOpenFile == fpOpenFile);
        delete fpOpenFile;
        fpOpenFile = NULL;
    }
    virtual bool IsFileOpen(void) const { return fpOpenFile != NULL; }

    virtual void SetParent(A2File* pParent) { fpParent = pParent; }
    virtual A2File* GetParent(void) const { return fpParent; }

    static char NameToLower(char ch);
    static void InitDirEntry(DirEntry* pEntry,
        const unsigned char* entryBuf);

    virtual void Dump(void) const;

    /* directory entry contents for this file */
    DirEntry        fDirEntry;

    /* pointer to directory entry (update dir if file size or dates change) */
    unsigned short  fParentDirBlock;    // directory block
    int             fParentDirIdx;      // index in dir block

    /* these are only valid if storageType == kStorageExtended */
    ExtendedInfo    fExtData;
    ExtendedInfo    fExtRsrc;

    void SetPathName(const char* basePath, const char* fileName);
    static time_t ConvertProDate(ProDate proDate);
    static ProDate ConvertProDate(time_t unixDate);

    /* returns "true" if AppleWorks aux type is used for lower-case name */
    static bool UsesAppleWorksAuxType(unsigned char fileType) {
        return (fileType >= 0x19 && fileType <= 0x1b);
    }

#if 0
    /* change fPathName; should only be used by DiskFS rename */
    void SetPathName(const char* name) {
        delete[] fPathName;
        if (name == NULL) {
            fPathName = NULL;
        } else {
            fPathName = new char[strlen(name)+1];
            if (fPathName != NULL)
                strcpy(fPathName, name);
        }
    }
#endif

    DIError LoadBlockList(int storageType, unsigned short keyBlock,
        long eof, long* pBlockCount, unsigned short** pBlockList,
        long* pIndexBlockCount=NULL, unsigned short** pIndexBlockList=NULL);
    DIError LoadDirectoryBlockList(unsigned short keyBlock,
        long eof, long* pBlockCount, unsigned short** pBlockList);

    /* fork lengths without sparseness */
    di_off_t        fSparseDataEof;
    di_off_t        fSparseRsrcEof;

private:
    DIError LoadIndexBlock(unsigned short block, unsigned short* list,
        int maxCount);
    DIError ValidateBlockList(const unsigned short* list, long count);

    char*           fPathName;      // full pathname to file on this volume

    A2FDProDOS*     fpOpenFile;     // only one fork can be open at a time
    A2File*         fpParent;
};


/*
 * ===========================================================================
 *      Pascal
 * ===========================================================================
 */

/*
 * Pascal disk.
 *
 * There is no allocation map or file index blocks, just a linear collection
 * of files with contiguous blocks.
 */
class A2FilePascal;

class DISKIMG_API DiskFSPascal : public DiskFS {
public:
    DiskFSPascal(void) : fDirectory(NULL) {}
    virtual ~DiskFSPascal(void) {
        if (fDirectory != NULL) {
            assert(false);      // unexpected
            delete[] fDirectory;
        }
    }

    static DIError TestFS(DiskImg* pImg, DiskImg::SectorOrder* pOrder,
        DiskImg::FSFormat* pFormat, FSLeniency leniency);

    virtual DIError Initialize(DiskImg* pImg, InitMode initMode) {
        SetDiskImg(pImg);
        return Initialize();
    }
    virtual DIError Format(DiskImg* pDiskImg, const char* volName);

    // assorted constants
    enum {
        kMaxVolumeName = 7,
        kDirectoryEntryLen = 26,
    };
    typedef unsigned short PascalDate;

    virtual const char* GetVolumeName(void) const { return fVolumeName; }
    virtual const char* GetVolumeID(void) const { return fVolumeID; }
    virtual const char* GetBareVolumeName(void) const { return fVolumeName; }
    virtual bool GetReadWriteSupported(void) const { return true; }
    virtual bool GetFSDamaged(void) const { return !fDiskIsGood; }
    virtual DIError GetFreeSpaceCount(long* pTotalUnits, long* pFreeUnits,
        int* pUnitSize) const;
    virtual DIError NormalizePath(const char* path, char fssep,
        char* normalizedBuf, int* pNormalizedBufLen);
    virtual DIError CreateFile(const CreateParms* pParms, A2File** ppNewFile);
    virtual DIError DeleteFile(A2File* pFile);
    virtual DIError RenameFile(A2File* pFile, const char* newName);
    virtual DIError SetFileInfo(A2File* pFile, long fileType, long auxType,
        long accessFlags);
    virtual DIError RenameVolume(const char* newName);

    static bool IsValidVolumeName(const char* name);
    static bool IsValidFileName(const char* name);

    unsigned short GetTotalBlocks(void) const { return fTotalBlocks; }

    friend class A2FDPascal;

private:
    DIError Initialize(void);
    DIError LoadVolHeader(void);
    void SetVolumeID(void);
    void DumpVolHeader(void);
    DIError LoadCatalog(void);
    DIError SaveCatalog(void);
    void FreeCatalog(void);
    DIError ProcessCatalog(void);
    DIError ScanFileUsage(void);
    void SetBlockUsage(long block, VolumeUsage::ChunkPurpose purpose);
    DIError WriteBootBlocks(void);
    bool CheckDiskIsGood(void);
    void DoNormalizePath(const char* name, char fssep, char* outBuf);
    DIError MakeFileNameUnique(char* fileName);
    DIError FindLargestFreeArea(int *pPrevIdx, A2FilePascal** ppPrevFile);
    unsigned char* FindDirEntry(A2FilePascal* pFile);

    enum { kMaxExtensionLen = 5 };  // used when normalizing; ".code" is 4

    /* some items from the volume header */
    unsigned short  fStartBlock;        // first block of dir hdr; always 2
    unsigned short  fNextBlock;         // i.e. first block with data
    char            fVolumeName[kMaxVolumeName+1];
    char            fVolumeID[kMaxVolumeName + 16]; // add "Pascal ___:"
    unsigned short  fTotalBlocks;
    unsigned short  fNumFiles;
    PascalDate      fAccessWhen;        // PascalDate last access
    PascalDate      fDateSetWhen;       // PascalDate last date setting
    unsigned short  fStuff1;            //
    unsigned short  fStuff2;            //

    /* other goodies */
    bool            fDiskIsGood;
    bool            fEarlyDamage;

    /*
     * Pascal disks have one fixed-size directory.  The contents aren't
     * divided into blocks, which means you can't always edit an entry
     * by loading a single block from disk and writing it back.  Also,
     * deleted entries are squeezed out, so if we delete an entry we
     * have to reshuffle the entries below it.
     *
     * We want to keep the copy on disk synced up, so we don't hold on
     * to this longer than necessary.  Possibly less efficient that way;
     * if it becomes a problem it's easy enough to change the behavior.
     */
    unsigned char*  fDirectory;
};

/*
 * File descriptor for an open Pascal file.
 */
class DISKIMG_API A2FDPascal : public A2FileDescr {
public:
    A2FDPascal(A2File* pFile) : A2FileDescr(pFile) {
        fOffset = 0;
    }
    virtual ~A2FDPascal(void) {
        /* nothing to clean up */
    }

    friend class A2FilePascal;

    virtual DIError Read(void* buf, size_t len, size_t* pActual = NULL);
    virtual DIError Write(const void* buf, size_t len, size_t* pActual = NULL);
    virtual DIError Seek(di_off_t offset, DIWhence whence);
    virtual di_off_t Tell(void);
    virtual DIError Close(void);

    virtual long GetSectorCount(void) const;
    virtual long GetBlockCount(void) const;
    virtual DIError GetStorage(long sectorIdx, long* pTrack, long* pSector) const;
    virtual DIError GetStorage(long blockIdx, long* pBlock) const;

private:
    di_off_t        fOffset;            // where we are
    di_off_t        fOpenEOF;           // how big the file currently is
    long            fOpenBlocksUsed;    // how many blocks it occupies
    bool            fModified;          // if modified, update dir on Close
};

/*
 * File on a Pascal disk.
 */
class DISKIMG_API A2FilePascal : public A2File {
public:
    A2FilePascal(DiskFS* pDiskFS) : A2File(pDiskFS) {
        fpOpenFile = NULL;
    }
    virtual ~A2FilePascal(void) {
        /* this comes back and calls CloseDescr */
        if (fpOpenFile != NULL)
            fpOpenFile->Close();
    }

    typedef DiskFSPascal::PascalDate PascalDate;

    // assorted constants
    enum {
        kMaxFileName = 15,
    };
    typedef enum FileType {
        kTypeUntyped = 0,   // NON
        kTypeXdsk = 1,      // BAD (bad blocks)
        kTypeCode = 2,      // PCD
        kTypeText = 3,      // PTX
        kTypeInfo = 4,      // ?
        kTypeData = 5,      // PDA
        kTypeGraf = 6,      // ?
        kTypeFoto = 7,      // FOT? (hires image)
        kTypeSecurdir = 8   // ??
    } FileType;

    /*
     * Implementations of standard interfaces.
     */
    virtual const char* GetFileName(void) const { return fFileName; }
    virtual const char* GetPathName(void) const { return fFileName; }
    virtual char GetFssep(void) const { return '\0'; }
    virtual long GetFileType(void) const;
    virtual long GetAuxType(void) const { return 0; }
    virtual long GetAccess(void) const { return DiskFS::kFileAccessUnlocked; }
    virtual time_t GetCreateWhen(void) const { return 0; }
    virtual time_t GetModWhen(void) const;
    virtual di_off_t GetDataLength(void) const { return fLength; }
    virtual di_off_t GetDataSparseLength(void) const { return fLength; }
    virtual di_off_t GetRsrcLength(void) const { return -1; }
    virtual di_off_t GetRsrcSparseLength(void) const { return -1; }

    virtual DIError Open(A2FileDescr** pOpenFile, bool readOnly,
        bool rsrcFork = false);
    virtual void CloseDescr(A2FileDescr* pOpenFile) {
        assert(pOpenFile == fpOpenFile);
        delete fpOpenFile;
        fpOpenFile = NULL;
    }
    virtual bool IsFileOpen(void) const { return fpOpenFile != NULL; }


    virtual void Dump(void) const;

    static time_t ConvertPascalDate(PascalDate pascalDate);
    static A2FilePascal::PascalDate ConvertPascalDate(time_t unixDate);
    static A2FilePascal::FileType ConvertFileType(long prodosType);

    /* fields pulled out of directory block */
    unsigned short  fStartBlock;
    unsigned short  fNextBlock;
    FileType        fFileType;
    char            fFileName[kMaxFileName+1];
    unsigned short  fBytesRemaining;
    PascalDate      fModWhen;

    /* derived fields */
    di_off_t        fLength;

    /* note to self: don't try to store a directory offset here; they shift
       every time you add or delete a file */

private:
    A2FileDescr*    fpOpenFile;
};


/*
 * ===========================================================================
 *      CP/M
 * ===========================================================================
 */

/*
 * CP/M disk.
 *
 * We really ought to be using 1K blocks here, since that's the native
 * CP/M format, but there's little value in making an exception for such
 * a rarely used Apple II format.
 *
 * There is no allocation map or file index blocks, just a single 2K
 * directory filled with files that have up to 16 1K blocks each.  If
 * a file is longer than 16K, a second entry with the identical name
 * and user number is made.  These "extents" may be sparse, so it's
 * necessary to use the "records" field to determine the actual file length.
 */
class A2FileCPM;
class DISKIMG_API DiskFSCPM : public DiskFS {
public:
    DiskFSCPM(void) : fDiskIsGood(false) {}
    virtual ~DiskFSCPM(void) {}

    static DIError TestFS(DiskImg* pImg, DiskImg::SectorOrder* pOrder,
        DiskImg::FSFormat* pFormat, FSLeniency leniency);

    virtual DIError Initialize(DiskImg* pImg, InitMode initMode) {
        SetDiskImg(pImg);
        return Initialize();
    }

    virtual const char* GetVolumeName(void) const { return "CP/M"; }
    virtual const char* GetVolumeID(void) const { return "CP/M"; }
    virtual const char* GetBareVolumeName(void) const { return NULL; }
    virtual bool GetReadWriteSupported(void) const { return false; }
    virtual bool GetFSDamaged(void) const { return !fDiskIsGood; }
    virtual DIError GetFreeSpaceCount(long* pTotalUnits, long* pFreeUnits,
        int* pUnitSize) const
        { return kDIErrNotSupported; }

    // assorted constants
    enum {
        kDirectoryEntryLen = 32,
        kVolDirBlock = 24,          // ProDOS block where volume dir starts
        kDirFileNameLen = 11,       // 8+3 without the '.'
        kFullDirSize = 2048,        // blocks 0 and 1
        kDirEntryBlockCount = 16,   // #of blocks held in dir slot
        kNumDirEntries = kFullDirSize/kDirectoryEntryLen,
        kExtentsInLowByte = 32,

        kDirEntryFlagContinued = 0x8000,    // "flags" word
    };
    // Contents of the raw 32-byte directory entry.
    //
    // From http://www.seasip.demon.co.uk/Cpm/format31.html
    //
    // UU F1 F2 F3 F4 F5 F6 F7 F8 T1 T2 T3 EX S1 S2 RC   .FILENAMETYP....
    // AL AL AL AL AL AL AL AL AL AL AL AL AL AL AL AL   ................
    //
    // If the high bit of T1 is set, the file is read-only.  If the high
    // bit of T2 is set, the file is a "system" file.
    //
    // An entry with UU=0x20 indicates a CP/M 3.1 disk label entry.
    // An entry with UU=0x21 indicates a time stamp entry (2.x or 3.x).
    //
    // Files larger than (1024 * 16) have multiple "extent" entries, i.e.
    // entries with the same user number and file name.
    typedef struct DirEntry {
        unsigned char   userNumber; // 0-15 or 0-31 (usually 0), e5=unused
        unsigned char   fileName[kDirFileNameLen+1];
        unsigned short  extent;     // extent (EX + S2 * 32)
        unsigned char   S1;         // Last Record Byte Count (app-specific)
        unsigned char   records;    // #of 128-byte records in this extent
        unsigned char   blocks[kDirEntryBlockCount];
        bool            readOnly;
        bool            system;
        bool            badBlockList;   // set if block list is damaged
    } DirEntry;

    static long CPMToProDOSBlock(long cpmBlock) {
        return kVolDirBlock + (cpmBlock*2);
    }

private:
    DIError Initialize(void);
    DIError ReadCatalog(void);
    DIError ScanFileUsage(void);
    void SetBlockUsage(long block, VolumeUsage::ChunkPurpose purpose);
    void FormatName(char* dstBuf, const char* srcBuf);
    DIError ComputeLength(A2FileCPM* pFile);
    bool CheckDiskIsGood(void);

    // the full set of raw dir entries
    DirEntry        fDirEntry[kNumDirEntries];

    bool    fDiskIsGood;
};

/*
 * File descriptor for an open CP/M file.
 */
class DISKIMG_API A2FDCPM : public A2FileDescr {
public:
    A2FDCPM(A2File* pFile) : A2FileDescr(pFile) {
        //fOpen = false;
        fBlockList = NULL;
    }
    virtual ~A2FDCPM(void) {
        delete fBlockList;
        fBlockList = NULL;
    }

    friend class A2FileCPM;

    virtual DIError Read(void* buf, size_t len, size_t* pActual = NULL);
    virtual DIError Write(const void* buf, size_t len, size_t* pActual = NULL);
    virtual DIError Seek(di_off_t offset, DIWhence whence);
    virtual di_off_t Tell(void);
    virtual DIError Close(void);

    virtual long GetSectorCount(void) const;
    virtual long GetBlockCount(void) const;
    virtual DIError GetStorage(long sectorIdx, long* pTrack, long* pSector) const;
    virtual DIError GetStorage(long blockIdx, long* pBlock) const;

private:
    //bool          fOpen;
    di_off_t        fOffset;
    long            fBlockCount;
    unsigned char*  fBlockList;
};

/*
 * File on a CP/M disk.
 */
class DISKIMG_API A2FileCPM : public A2File {
public:
    typedef DiskFSCPM::DirEntry DirEntry;

    A2FileCPM(DiskFS* pDiskFS, DirEntry* pDirEntry) :
        A2File(pDiskFS), fpDirEntry(pDirEntry)
    {
        fDirIdx = -1;
        fpOpenFile = NULL;
    }
    virtual ~A2FileCPM(void) {
        delete fpOpenFile;
    }

    // assorted constants
    enum {
        kMaxFileName = 12,      // 8+3 including '.'
    };

    /*
     * Implementations of standard interfaces.
     */
    virtual const char* GetFileName(void) const { return fFileName; }
    virtual const char* GetPathName(void) const { return fFileName; }
    virtual char GetFssep(void) const { return '\0'; }
    virtual long GetFileType(void) const { return 0; }
    virtual long GetAuxType(void) const { return 0; }
    virtual long GetAccess(void) const {
        if (fReadOnly)
            return DiskFS::kFileAccessLocked;
        else
            return DiskFS::kFileAccessUnlocked;
    }
    virtual time_t GetCreateWhen(void) const { return 0; }
    virtual time_t GetModWhen(void) const { return 0; }
    virtual di_off_t GetDataLength(void) const { return fLength; }
    virtual di_off_t GetDataSparseLength(void) const { return fLength; }
    virtual di_off_t GetRsrcLength(void) const { return -1; }
    virtual di_off_t GetRsrcSparseLength(void) const { return -1; }

    virtual DIError Open(A2FileDescr** ppOpenFile, bool readOnly,
        bool rsrcFork = false);
    virtual void CloseDescr(A2FileDescr* pOpenFile) {
        assert(pOpenFile == fpOpenFile);
        delete fpOpenFile;
        fpOpenFile = NULL;
    }
    virtual bool IsFileOpen(void) const { return fpOpenFile != NULL; }

    virtual void Dump(void) const;

    /* fields pulled out of directory block */
    char            fFileName[kMaxFileName+1];
    bool            fReadOnly;

    /* derived fields */
    di_off_t        fLength;
    int             fDirIdx;        // index into fDirEntry for part #1

    DIError GetBlockList(long* pBlockCount, unsigned char* blockBuf) const;

private:
    const DirEntry* fpDirEntry;
    A2FileDescr*    fpOpenFile;
};


/*
 * ===========================================================================
 *      RDOS
 * ===========================================================================
 */

/*
 * RDOS disk.
 *
 * There is no allocation map or file index blocks, just a linear collection
 * of files with contiguous sectors.  Very similar to Pascal.
 *
 * The one interesting quirk is the "converted 13-sector disk" format, where
 * only 13 of 16 sectors are actually used.  The linear sector addressing
 * must take that into account.
 */
class A2FileRDOS;
class DISKIMG_API DiskFSRDOS : public DiskFS {
public:
    DiskFSRDOS(void) {}
    virtual ~DiskFSRDOS(void) {}

    static DIError TestFS(DiskImg* pImg, DiskImg::SectorOrder* pOrder,
        DiskImg::FSFormat* pFormat, FSLeniency leniency);

    virtual DIError Initialize(DiskImg* pImg, InitMode initMode) {
        SetDiskImg(pImg);
        return Initialize();
    }

    virtual const char* GetVolumeName(void) const { return fVolumeName; }
    virtual const char* GetVolumeID(void) const { return fVolumeName; }
    virtual const char* GetBareVolumeName(void) const { return NULL; }
    virtual bool GetReadWriteSupported(void) const { return false; }
    virtual bool GetFSDamaged(void) const { return false; }
    virtual DIError GetFreeSpaceCount(long* pTotalUnits, long* pFreeUnits,
        int* pUnitSize) const
        { return kDIErrNotSupported; }

    int GetOurSectPerTrack(void) const { return fOurSectPerTrack; }

private:
    static DIError TestCommon(DiskImg* pImg, DiskImg::SectorOrder* pOrder,
        FSLeniency leniency, DiskImg::FSFormat* pFormatFound);

    DIError Initialize(void);
    DIError ReadCatalog(void);
    DIError ScanFileUsage(void);
    void SetSectorUsage(long track, long sector,
        VolumeUsage::ChunkPurpose purpose);

    char    fVolumeName[10];        // e.g. "RDOS 3.3"
    int     fOurSectPerTrack;
};

/*
 * File descriptor for an open RDOS file.
 */
class DISKIMG_API A2FDRDOS : public A2FileDescr {
public:
    A2FDRDOS(A2File* pFile) : A2FileDescr(pFile) {
        fOffset = 0;
    }
    virtual ~A2FDRDOS(void) {
        /* nothing to clean up */
    }

    friend class A2FileRDOS;

    virtual DIError Read(void* buf, size_t len, size_t* pActual = NULL);
    virtual DIError Write(const void* buf, size_t len, size_t* pActual = NULL);
    virtual DIError Seek(di_off_t offset, DIWhence whence);
    virtual di_off_t Tell(void);
    virtual DIError Close(void);

    virtual long GetSectorCount(void) const;
    virtual long GetBlockCount(void) const;
    virtual DIError GetStorage(long sectorIdx, long* pTrack, long* pSector) const;
    virtual DIError GetStorage(long blockIdx, long* pBlock) const;

private:
    /* RDOS is unique in that it can put 13-sector disks on 16-sector tracks */
    inline int GetOurSectPerTrack(void) const {
        DiskFSRDOS* pDiskFS = (DiskFSRDOS*) fpFile->GetDiskFS();
        return pDiskFS->GetOurSectPerTrack();
    }

    //bool          fOpen;
    di_off_t        fOffset;
};

/*
 * File on an RDOS disk.
 */
class DISKIMG_API A2FileRDOS : public A2File {
public:
    A2FileRDOS(DiskFS* pDiskFS) : A2File(pDiskFS) {
        //fOpen = false;
        fpOpenFile = NULL;
    }
    virtual ~A2FileRDOS(void) {
        delete fpOpenFile;
    }

    // assorted constants
    enum {
        kMaxFileName = 24,
    };
    typedef enum FileType {
        kTypeUnknown = 0,
        kTypeApplesoft,             // 'A'
        kTypeBinary,                // 'B'
        kTypeText,                  // 'T'
    } FileType;

    /*
     * Implementations of standard interfaces.
     */
    virtual const char* GetFileName(void) const { return fFileName; }
    virtual const char* GetPathName(void) const { return fFileName; }
    virtual char GetFssep(void) const { return '\0'; }
    virtual long GetFileType(void) const;
    virtual long GetAuxType(void) const { return fLoadAddr; }
    virtual long GetAccess(void) const { return DiskFS::kFileAccessUnlocked; }
    virtual time_t GetCreateWhen(void) const { return 0; }
    virtual time_t GetModWhen(void) const { return 0; };
    virtual di_off_t GetDataLength(void) const { return fLength; }
    virtual di_off_t GetDataSparseLength(void) const { return fLength; }
    virtual di_off_t GetRsrcLength(void) const { return -1; }
    virtual di_off_t GetRsrcSparseLength(void) const { return -1; }

    virtual DIError Open(A2FileDescr** ppOpenFile, bool readOnly,
        bool rsrcFork = false);
    virtual void CloseDescr(A2FileDescr* pOpenFile) {
        assert(pOpenFile == fpOpenFile);
        delete fpOpenFile;
        fpOpenFile = NULL;
    }
    virtual bool IsFileOpen(void) const { return fpOpenFile != NULL; }

    void FixFilename(void);
    virtual void Dump(void) const;

    /* fields pulled out of directory block */
    char            fFileName[kMaxFileName+1];
    FileType        fFileType;
    unsigned short  fNumSectors;
    unsigned short  fLoadAddr;
    unsigned short  fLength;
    unsigned short  fStartSector;

private:
    void TrimTrailingSpaces(char* filename);

    A2FileDescr*    fpOpenFile;
};


/*
 * ===========================================================================
 *      HFS
 * ===========================================================================
 */

/*
 * HFS disk.
 */
class A2FileHFS;
class DISKIMG_API DiskFSHFS : public DiskFS {
public:
    DiskFSHFS(void) {
        fLocalTimeOffset = -1;
        fDiskIsGood = true;
#ifndef EXCISE_GPL_CODE
        fHfsVol = NULL;
#endif
    }
    virtual ~DiskFSHFS(void) {
#ifndef EXCISE_GPL_CODE
        hfs_callback_close(fHfsVol);
        fHfsVol = (hfsvol*) 0xcdaaaacd;
#endif
    }

    static DIError TestFS(DiskImg* pImg, DiskImg::SectorOrder* pOrder,
        DiskImg::FSFormat* pFormat, FSLeniency leniency);

    virtual DIError Initialize(DiskImg* pImg, InitMode initMode) {
        SetDiskImg(pImg);
        return Initialize(initMode);
    }

#ifndef EXCISE_GPL_CODE
    /* these are optional, defined as no-ops in the parent class */
    virtual DIError Format(DiskImg* pDiskImg, const char* volName);
    virtual DIError NormalizePath(const char* path, char fssep,
        char* normalizedBuf, int* pNormalizedBufLen);
    virtual DIError CreateFile(const CreateParms* pParms, A2File** ppNewFile);
    virtual DIError DeleteFile(A2File* pFile);
    virtual DIError RenameFile(A2File* pFile, const char* newName);
    virtual DIError SetFileInfo(A2File* pFile, long fileType, long auxType,
        long accessFlags);
    virtual DIError RenameVolume(const char* newName);
#endif

    // assorted constants
    enum {
        kMaxVolumeName = 27,
        kMaxExtensionLen = 4,   // used when normalizing; ".gif" is 4
    };

    /* mandatory functions */
    virtual const char* GetVolumeName(void) const { return fVolumeName; }
    virtual const char* GetVolumeID(void) const { return fVolumeID; }
    virtual const char* GetBareVolumeName(void) const { return fVolumeName; }
    virtual bool GetReadWriteSupported(void) const { return true; }
    virtual bool GetFSDamaged(void) const { return false; }
    virtual long GetFSNumBlocks(void) const { return fTotalBlocks; }
    virtual DIError GetFreeSpaceCount(long* pTotalUnits, long* pFreeUnits,
        int* pUnitSize) const;

#ifndef EXCISE_GPL_CODE
    hfsvol* GetHfsVol(void) const { return fHfsVol; }
#endif

    // utility function, used by app
    static bool IsValidVolumeName(const char* name);
    static bool IsValidFileName(const char* name);

private:
    enum {
        // Macintosh 32-bit dates start in 1904, everybody else starts in
        // 1970.  Take the Mac date and adjust it 66 years plus 17 leap days.
        // The annoying part is that HFS stores dates in local time, which
        // means it's impossible to know absolutely when a file was modified.
        // libhfs converts timestamps to the current time zone, so that a
        // file written January 1st 2006 at 6pm in London will appear to have
        // been written January 1st 2006 at 6pm in San Francisco if you
        // happen to be sitting in California.
        //
        // This was fixed in HFS+, but we have to deal with it for now.  The
        // value below converts the date to local time in Greenwich; the
        // current GMT offset and daylight saving time must be added to it.
        //
        // Curiously, the volume dates shown by Cmd-I on the volume on my
        // Quadra are off by an hour, even though the file dates match.
        kDateTimeOffset = (1970 - 1904) * 60 * 60 * 24 * 365 +
                            (60 * 60 * 24 * 17),

        kExpectedMinBlocks = 1440,      // ignore volumes under 720K
    };

    struct MasterDirBlock;      // fwd
    static void UnpackMDB(const unsigned char* buf, MasterDirBlock* pMDB);
    static DIError TestImage(DiskImg* pImg, DiskImg::SectorOrder imageOrder);

    DIError Initialize(InitMode initMode);
    DIError LoadVolHeader(void);
    void SetVolumeID(void);
    void DumpVolHeader(void);
    void SetVolumeUsageMap(void);

#ifdef EXCISE_GPL_CODE
    void CreateFakeFile(void);
#else
    DIError RecursiveDirAdd(A2File* pParent, const char* basePath, int depth);
    //void Sanitize(unsigned char* str);
    DIError DoNormalizePath(const char* path, char fssep,
        char** pNormalizedPath);
    static int CompareMacFileNames(const char* str1, const char* str2);
    DIError RegeneratePathName(A2FileHFS* pFile);
    DIError MakeFileNameUnique(const char* pathName, char** pUniqueName);

    /* libhfs stuff */
    static unsigned long LibHFSCB(void* vThis, int op, unsigned long arg1,
        void* arg2);
    hfsvol*         fHfsVol;
#endif


    /* some items from the volume header */
    char            fVolumeName[kMaxVolumeName+1];
    char            fVolumeID[kMaxVolumeName + 8];  // add "HFS :"
    unsigned long   fTotalBlocks;
    unsigned long   fAllocationBlockSize;
    unsigned long   fNumAllocationBlocks;
    unsigned long   fCreatedDateTime;
    unsigned long   fModifiedDateTime;
    unsigned long   fNumFiles;
    unsigned long   fNumDirectories;

    long            fLocalTimeOffset;
    bool            fDiskIsGood;
};

/*
 * File descriptor for an open HFS file.
 */
class DISKIMG_API A2FDHFS : public A2FileDescr {
public:
#ifdef EXCISE_GPL_CODE
    A2FDHFS(A2File* pFile, void* unused)
        : A2FileDescr(pFile), fOffset(0)
    {}
#else
    A2FDHFS(A2File* pFile, hfsfile* pHfsFile)
        : A2FileDescr(pFile), fHfsFile(pHfsFile), fModified(false)
    {}
#endif
    virtual ~A2FDHFS(void) {
#ifndef EXCISE_GPL_CODE
        if (fHfsFile != NULL)
            hfs_close(fHfsFile);
#endif
    }

    friend class A2FileHFS;

    virtual DIError Read(void* buf, size_t len, size_t* pActual = NULL);
    virtual DIError Write(const void* buf, size_t len, size_t* pActual = NULL);
    virtual DIError Seek(di_off_t offset, DIWhence whence);
    virtual di_off_t Tell(void);
    virtual DIError Close(void);

    virtual long GetSectorCount(void) const;
    virtual long GetBlockCount(void) const;
    virtual DIError GetStorage(long sectorIdx, long* pTrack, long* pSector) const;
    virtual DIError GetStorage(long blockIdx, long* pBlock) const;

private:
#ifdef EXCISE_GPL_CODE
    di_off_t    fOffset;
#else
    hfsfile*    fHfsFile;
    bool        fModified;
#endif
};

/*
 * File on an HFS disk.
 */
class DISKIMG_API A2FileHFS : public A2File {
public:
    A2FileHFS(DiskFS* pDiskFS) : A2File(pDiskFS) {
        fPathName = NULL;
        fpOpenFile = NULL;
#ifdef EXCISE_GPL_CODE
        fFakeFileBuf = NULL;
#else
        //fOrigPathName = NULL;
#endif
    }
    virtual ~A2FileHFS(void) {
        delete fpOpenFile;
        delete[] fPathName;
#ifdef EXCISE_GPL_CODE
        delete[] fFakeFileBuf;
#else
        //delete[] fOrigPathName;
#endif
    }

    /*
     * Implementations of standard interfaces.
     */
    virtual const char* GetFileName(void) const { return fFileName; }
    virtual const char* GetPathName(void) const { return fPathName; }
    virtual char GetFssep(void) const { return kFssep; }
    virtual long GetFileType(void) const;
    virtual long GetAuxType(void) const;
    virtual long GetAccess(void) const { return fAccess; }
    virtual time_t GetCreateWhen(void) const { return fCreateWhen; }
    virtual time_t GetModWhen(void) const { return fModWhen; }
    virtual di_off_t GetDataLength(void) const { return fDataLength; }
    virtual di_off_t GetDataSparseLength(void) const { return fDataLength; }
    virtual di_off_t GetRsrcLength(void) const { return fRsrcLength; }
    virtual di_off_t GetRsrcSparseLength(void) const { return fRsrcLength; }
    virtual bool IsDirectory(void) const { return fIsDir; }
    virtual bool IsVolumeDirectory(void) const { return fIsVolumeDir; }

    virtual DIError Open(A2FileDescr** pOpenFile, bool readOnly,
        bool rsrcFork = false);
    virtual void CloseDescr(A2FileDescr* pOpenFile) {
        assert(pOpenFile == fpOpenFile);
        delete fpOpenFile;
        fpOpenFile = NULL;
    }
    virtual bool IsFileOpen(void) const { return fpOpenFile != NULL; }

    enum {
        kMaxFileName = 31,
        kFssep = ':',
        kPdosType = 0x70646f73,     // 'pdos'
    };

    void SetPathName(const char* basePath, const char* fileName);
    virtual void Dump(void) const;

#ifdef EXCISE_GPL_CODE
    void SetFakeFile(void* buf, long len) {
        assert(len > 0);
        if (fFakeFileBuf != NULL)
            delete[] fFakeFileBuf;
        fFakeFileBuf = new char[len];
        memcpy(fFakeFileBuf, buf, len);
        fDataLength = len;
    }
    const void* GetFakeFileBuf(void) const { return fFakeFileBuf; }
#else
    void InitEntry(const hfsdirent* dirEntry);
    void SetOrigPathName(const char* pathName);
    virtual void SetParent(A2File* pParent) { fpParent = pParent; }
    virtual A2File* GetParent(void) const { return fpParent; }
    char* GetLibHFSPathName(void) const;
    static void ConvertTypeToHFS(long fileType, long auxType,
        char* pType, char* pCreator);
#endif

    bool            fIsDir;
    bool            fIsVolumeDir;
    long            fType;
    long            fCreator;
    char            fFileName[kMaxFileName+1];
    char*           fPathName;
    di_off_t        fDataLength;
    di_off_t        fRsrcLength;
    time_t          fCreateWhen;
    time_t          fModWhen;
    long            fAccess;

private:
#ifdef EXCISE_GPL_CODE
    char*           fFakeFileBuf;
#else
    //char*         fOrigPathName;
    A2File*         fpParent;
#endif
    A2FileDescr*    fpOpenFile;     // only one fork can be open at a time
};


/*
 * ===========================================================================
 *      Gutenberg
 * ===========================================================================
 */

class A2FileGutenberg;

/*
 * Gutenberg disk.
 */
class DISKIMG_API DiskFSGutenberg : public DiskFS {
public:
    DiskFSGutenberg(void) : DiskFS() {
        fVTOCLoaded = false;
        fDiskIsGood = false;
    }
    virtual ~DiskFSGutenberg(void) {}

    static DIError TestFS(DiskImg* pImg, DiskImg::SectorOrder* pOrder,
        DiskImg::FSFormat* pFormat, FSLeniency leniency);

    virtual DIError Initialize(DiskImg* pImg, InitMode initMode) {
        SetDiskImg(pImg);
        return Initialize(initMode);
    }

    virtual const char* GetVolumeName(void) const { return fDiskVolumeName; }
    virtual const char* GetVolumeID(void) const { return fDiskVolumeID; }
    virtual const char* GetBareVolumeName(void) const {
        return fDiskVolumeName;
    }
    virtual bool GetReadWriteSupported(void) const { return true; }
    virtual bool GetFSDamaged(void) const { return !fDiskIsGood; }
    virtual DIError GetFreeSpaceCount(long* pTotalUnits, long* pFreeUnits,
        int* pUnitSize) const;

    static bool IsValidFileName(const char* name);
    static bool IsValidVolumeName(const char* name);

    // utility function
    static void LowerASCII(unsigned char* buf, long len);
    static void ReplaceFssep(char* str, char replacement);

    enum {
        kMinTracks = 17,            // need to put the catalog track here
        kMaxTracks = 50,
        kMaxCatalogSectors = 64,    // two tracks on a 32-sector disk
    };

    /* a T/S pair */
    typedef struct TrackSector {
        char    track;
        char    sector;
    } TrackSector;

    friend class A2FDGutenberg; // for Write

private:
    DIError Initialize(InitMode initMode);
    DIError ReadVTOC(void);
    void UpdateVolumeNum(void);
    void DumpVTOC(void);
    void SetSectorUsage(long track, long sector,
        VolumeUsage::ChunkPurpose purpose);
    void FixVolumeUsageMap(void);
    DIError ReadCatalog(void);
    DIError ProcessCatalogSector(int catTrack, int catSect,
        const unsigned char* sctBuf);
    DIError GetFileLengths(void);
    DIError ComputeLength(A2FileGutenberg* pFile, const TrackSector* tsList,
        int tsCount);
    DIError TrimLastSectorUp(A2FileGutenberg* pFile, TrackSector lastTS);
    void MarkFileUsage(A2FileGutenberg* pFile, TrackSector* tsList, int tsCount,
        TrackSector* indexList, int indexCount);
    DIError MakeFileNameUnique(char* fileName);
    DIError GetFreeCatalogEntry(TrackSector* pCatSect, int* pCatEntry,
        unsigned char* sctBuf, A2FileGutenberg** ppPrevEntry);
    void CreateDirEntry(unsigned char* sctBuf, int catEntry,
        const char* fileName, TrackSector* pTSSect, unsigned char fileType,
        int access);
    void FreeTrackSectors(TrackSector* pList, int count);

    bool CheckDiskIsGood(void);

    DIError WriteDOSTracks(int sectPerTrack);

    DIError ScanVolBitmap(void);
    DIError LoadVolBitmap(void);
    DIError SaveVolBitmap(void);
    void FreeVolBitmap(void);
    DIError AllocSector(TrackSector* pTS);
    DIError CreateEmptyBlockMap(bool withDOS);
    bool GetSectorUseEntry(long track, int sector) const;
    void SetSectorUseEntry(long track, int sector, bool inUse);
    inline unsigned long GetVTOCEntry(const unsigned char* pVTOC,
        long track) const;

    // Largest interesting volume is 400K (50 tracks, 32 sectors), but
    // we may be looking at it in 16-sector mode, so max tracks is 100.
    enum {
        kMaxInterestingTracks = 100,
        kSectorSize = 256,
        kDefaultVolumeNum = 254,
        kMaxExtensionLen = 4,   // used when normalizing; ".gif" is 4
    };

    /* some fields from the VTOC */
    int     fFirstCatTrack;
    int     fFirstCatSector;
    int     fVTOCVolumeNumber;
    int     fVTOCNumTracks;
    int     fVTOCNumSectors;

    /* private data */
    char    fDiskVolumeName[10];        // 
    char    fDiskVolumeID[11+12+1];     // sizeof "Gutenberg: " + 12 + null
    unsigned char   fVTOC[kSectorSize];
    bool    fVTOCLoaded;

    /*
     * There are some things we need to be careful of when reading the
     * catalog track, like bad links and infinite loops.  By storing a list
     * of known good catalog sectors, we only have to handle that stuff once.
     * The catalog doesn't grow or shrink, so this never needs to be updated.
     */
    TrackSector fCatalogSectors[kMaxCatalogSectors];

    bool    fDiskIsGood;
};

/*
 * File descriptor for an open Gutenberg file.
 */
class DISKIMG_API A2FDGutenberg : public A2FileDescr {
public:
    A2FDGutenberg(A2File* pFile) : A2FileDescr(pFile) {
        fOffset = 0;
        fModified = false;
    }
    virtual ~A2FDGutenberg(void) {
    }

    friend class A2FileGutenberg;

    virtual DIError Read(void* buf, size_t len, size_t* pActual = NULL);
    virtual DIError Write(const void* buf, size_t len, size_t* pActual = NULL);
    virtual DIError Seek(di_off_t offset, DIWhence whence);
    virtual di_off_t Tell(void);
    virtual DIError Close(void);

    virtual long GetSectorCount(void) const;
    virtual long GetBlockCount(void) const;
    virtual DIError GetStorage(long sectorIdx, long* pTrack, long* pSector) const;
    virtual DIError GetStorage(long blockIdx, long* pBlock) const;

private:
    typedef DiskFSGutenberg::TrackSector TrackSector;

    int             fTSCount;
    di_off_t        fOffset;            // current position in file

    di_off_t        fOpenEOF;           // how big the file currently is
    long            fOpenSectorsUsed;   // how many sectors it occupies
    bool            fModified;          // if modified, update stuff on Close

    void DumpTSList(void) const;
};

/*
 * Holds Gutenberg files.
 *
 */
class DISKIMG_API A2FileGutenberg : public A2File {
public:
    A2FileGutenberg(DiskFS* pDiskFS);
    virtual ~A2FileGutenberg(void);

    // assorted constants
    enum {
        kMaxFileName = 12,
    };
    typedef enum {
        kTypeText           = 0x00,     // 'T'
    } FileType;

    /*
     * Implementations of standard interfaces.
     */
    virtual const char* GetFileName(void) const { return fFileName; }
    virtual const char* GetPathName(void) const { return fFileName; }
    virtual char GetFssep(void) const { return '\0'; }
    virtual long GetFileType(void) const;
    virtual long GetAuxType(void) const { return fAuxType; }
    virtual long GetAccess(void) const { return DiskFS::kFileAccessUnlocked; }
    virtual time_t GetCreateWhen(void) const { return 0; }
    virtual time_t GetModWhen(void) const { return 0; }
    virtual di_off_t GetDataLength(void) const { return fLength; }
    virtual di_off_t GetDataSparseLength(void) const { return fSparseLength; }
    virtual di_off_t GetRsrcLength(void) const { return -1; }
    virtual di_off_t GetRsrcSparseLength(void) const { return -1; }

    virtual DIError Open(A2FileDescr** ppOpenFile, bool readOnly,
        bool rsrcFork = false);
    virtual void CloseDescr(A2FileDescr* pOpenFile) {
        assert(pOpenFile == fpOpenFile);
        delete fpOpenFile;
        fpOpenFile = NULL;
    }
    virtual bool IsFileOpen(void) const { return fpOpenFile != NULL; }

    void Dump(void) const;

    typedef DiskFSGutenberg::TrackSector TrackSector;

    /*
     * Contents of directory entry.
     *
     * We don't hold deleted or unused entries, so fTSListTrack is always
     * valid.
     */
    short       fTrack;     // (could use TrackSector here)
    short       fSector;
    unsigned short  fLengthInSectors;
    bool        fLocked;
    char        fFileName[kMaxFileName+1];  // "fixed" version
    FileType    fFileType;

    TrackSector fCatTS;         // track/sector for our catalog entry
    int         fCatEntryNum;   // entry number within cat sector

    // these are computed or determined from the file contents
    unsigned short  fAuxType;           // addr for bin, etc.
    short           fDataOffset;        // for 'A'/'B'/'I' with embedded len
    di_off_t        fLength;            // file length, in bytes
    di_off_t        fSparseLength;      // file length, factoring sparse out

    void FixFilename(void);

    static FileType ConvertFileType(long prodosType, di_off_t fileLen);
    static bool IsValidType(long prodosType);
    static void MakeDOSName(char* buf, const char* name);
    static void TrimTrailingSpaces(char* filename);

private:
    DIError ExtractTSPairs(const unsigned char* sctBuf, TrackSector* tsList,
        int* pLastNonZero);

    A2FDGutenberg*      fpOpenFile;
};


/*
 * ===========================================================================
 *      FAT (including FAT12, FAT16, and FAT32)
 * ===========================================================================
 */

/*
 * MS-DOS FAT disk.
 *
 * This is currently just the minimum necessary to properly recognize
 * the disk.
 */
class A2FileFAT;
class DISKIMG_API DiskFSFAT : public DiskFS {
public:
    DiskFSFAT(void) {}
    virtual ~DiskFSFAT(void) {}

    static DIError TestFS(DiskImg* pImg, DiskImg::SectorOrder* pOrder,
        DiskImg::FSFormat* pFormat, FSLeniency leniency);

    virtual DIError Initialize(DiskImg* pImg, InitMode initMode) {
        SetDiskImg(pImg);
        return Initialize();
    }

    // assorted constants
    enum {
        kMaxVolumeName = 11,
    };

    virtual const char* GetVolumeName(void) const { return fVolumeName; }
    virtual const char* GetVolumeID(void) const { return fVolumeID; }
    virtual const char* GetBareVolumeName(void) const { return fVolumeName; }
    virtual bool GetReadWriteSupported(void) const { return false; }
    virtual bool GetFSDamaged(void) const { return false; }
    virtual long GetFSNumBlocks(void) const { return fTotalBlocks; }
    virtual DIError GetFreeSpaceCount(long* pTotalUnits, long* pFreeUnits,
        int* pUnitSize) const
        { return kDIErrNotSupported; }

private:
    enum {
        kExpectedMinBlocks = 720,       // ignore volumes under 360K
    };

    struct MasterBootRecord;        // fwd
    struct BootSector;
    static bool UnpackMBR(const unsigned char* buf, MasterBootRecord* pOut);
    static bool UnpackBootSector(const unsigned char* buf, BootSector* pOut);
    static DIError TestImage(DiskImg* pImg, DiskImg::SectorOrder imageOrder);

    DIError Initialize(void);
    DIError LoadVolHeader(void);
    void DumpVolHeader(void);
    void SetVolumeUsageMap(void);
    void CreateFakeFile(void);

    /* some items from the volume header */
    char            fVolumeName[kMaxVolumeName+1];
    char            fVolumeID[kMaxVolumeName + 8];  // add "FAT %s:"
    unsigned long   fTotalBlocks;
};

/*
 * File descriptor for an open FAT file.
 */
class DISKIMG_API A2FDFAT : public A2FileDescr {
public:
    A2FDFAT(A2File* pFile) : A2FileDescr(pFile) {
        fOffset = 0;
    }
    virtual ~A2FDFAT(void) {
        /* nothing to clean up */
    }

    friend class A2FileFAT;

    virtual DIError Read(void* buf, size_t len, size_t* pActual = NULL);
    virtual DIError Write(const void* buf, size_t len, size_t* pActual = NULL);
    virtual DIError Seek(di_off_t offset, DIWhence whence);
    virtual di_off_t Tell(void);
    virtual DIError Close(void);

    virtual long GetSectorCount(void) const;
    virtual long GetBlockCount(void) const;
    virtual DIError GetStorage(long sectorIdx, long* pTrack, long* pSector) const;
    virtual DIError GetStorage(long blockIdx, long* pBlock) const;

private:
    di_off_t        fOffset;
};

/*
 * File on a FAT disk.
 */
class DISKIMG_API A2FileFAT : public A2File {
public:
    A2FileFAT(DiskFS* pDiskFS) : A2File(pDiskFS) {
        fFakeFileBuf = NULL;
        //fFakeFileLen = -1;
        fpOpenFile = NULL;
    }
    virtual ~A2FileFAT(void) {
        delete fpOpenFile;
        delete[] fFakeFileBuf;
    }

    /*
     * Implementations of standard interfaces.
     */
    virtual const char* GetFileName(void) const { return fFileName; }
    virtual const char* GetPathName(void) const { return fFileName; }
    virtual char GetFssep(void) const { return '\0'; }
    virtual long GetFileType(void) const { return 0; };
    virtual long GetAuxType(void) const { return 0; }
    virtual long GetAccess(void) const { return DiskFS::kFileAccessUnlocked; }
    virtual time_t GetCreateWhen(void) const { return 0; }
    virtual time_t GetModWhen(void) const { return 0; }
    virtual di_off_t GetDataLength(void) const { return fLength; }
    virtual di_off_t GetDataSparseLength(void) const { return fLength; }
    virtual di_off_t GetRsrcLength(void) const { return -1; }
    virtual di_off_t GetRsrcSparseLength(void) const { return -1; }

    virtual DIError Open(A2FileDescr** pOpenFile, bool readOnly,
        bool rsrcFork = false);
    virtual void CloseDescr(A2FileDescr* pOpenFile) {
        assert(pOpenFile == fpOpenFile);
        delete fpOpenFile;
        fpOpenFile = NULL;
    }
    virtual bool IsFileOpen(void) const { return fpOpenFile != NULL; }

    enum { kMaxFileName = 31 };

    virtual void Dump(void) const;

    void SetFakeFile(void* buf, long len) {
        assert(len > 0);
        if (fFakeFileBuf != NULL)
            delete[] fFakeFileBuf;
        fFakeFileBuf = new char[len];
        memcpy(fFakeFileBuf, buf, len);
        fLength = len;
    }
    const void* GetFakeFileBuf(void) const { return fFakeFileBuf; }

    char            fFileName[kMaxFileName+1];
    di_off_t        fLength;

private:
    char*           fFakeFileBuf;
    //long          fFakeFileLen;
    A2FileDescr*    fpOpenFile;
};

};  // namespace DiskImgLib

#endif /*__DISKIMGDETAIL__*/
