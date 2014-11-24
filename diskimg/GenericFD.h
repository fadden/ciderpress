/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Declarations for GenericFD class and sub-classes.
 */
#ifndef DISKIMG_GENERICFD_H
#define DISKIMG_GENERICFD_H

#include "Win32BlockIO.h"

namespace DiskImgLib {

#if 0
/*
 * Embedded file descriptor class, representing an open file on a disk image.
 *
 * Useful for opening disk images that are stored as files inside of other
 * disk images.  For stuff like UNIDOS images, which don't have a file
 * associated with them, we can either open them as raw blocks, or create
 * a "fake" file to access them.  The latter is more general, and will work
 * for sub-volumes of sub-volumes.
 */
class DISKIMG_API EmbeddedFD {
public:
    EmbeddedFD(void) {
        fpDiskFS = NULL;
        fpA2File = NULL;
    }
    virtual ~EmbeddedFD(void) {}

    typedef enum Fork { kForkData = 0, kForkRsrc = 1 } Fork;
    // bit-flag values for Open call's "access" parameter
    enum {
        kAccessNone             = 0,    // somewhat useless
        kAccessRead             = 0x01, // O_RDONLY
        kAccessWrite            = 0x02, // O_WRONLY
        kAccessCreate           = 0x04, // O_CREAT
        kAccessMustNotExist     = 0x08, // O_EXCL, pointless w/o O_CREAT

        kAccessReadWrite        = (kAccessRead | kAccessWrite),
    };

    /*
     * Standard set of calls.
     */
    DIError Open(DiskFS* pDiskFS, const char* filename, Fork fork = kForkData,
        int access = kAccessRead, int fileCreatePerms = 0);
    DIError OpenBlocks(DiskFS* pDiskFS, long blockStart, long blockCount,
        int access = kAccessRead);
    DIError Read(void* buf, size_t length);
    DIError Write(const void* buf, size_t length);
    DIError Seek(di_off_t offset, DIWhence whence);
    DIError Close(void);

private:
    // prevent bitwise copying behavior
    EmbeddedFD& operator=(const EmbeddedFD&);
    EmbeddedFD(const EmbeddedFD&);

    DiskFS*     fpDiskFS;
    A2File*     fpA2File;
};
#endif


/*
 * Generic file source base class.  Allows us to treat files on disk, memory
 * buffers, and files embedded inside disk images equally.
 *
 * The file represented by the class is available in its entirety; skipping
 * past "wrapper headers" is expected to be done by the caller.
 *
 * The Read and Write calls take an optional parameter that allows the caller
 * to see how much data was actually read or written.  If the parameter is
 * not specified (or specified as NULL), then failure to return the exact
 * amount of data requested results an error.
 *
 * This is not meant to be the end-all of file wrapper classes; in
 * particular, it does not support file creation.
 *
 * Some libraries, such as NufxLib, require an actual filename to operate
 * (bad architecture?).  The GetPathName call will return the original
 * filename if one exists, or NULL if there isn't one.  (At which point the
 * caller has the option of creating a temp file, copying the data into
 * it, and cranking up NufxLib or zlib on that.)
 *
 * NOTE to self: see fsopen() to control sharing.
 *
 * NOTE: the Seek() implementations currently do not consistently allow or
 * disallow seeking past the current EOF of a file.  When writing a file this
 * can be very useful, so someday we should implement it for all classes.
 */
class GenericFD {
public:
    GenericFD(void) : fReadOnly(true) {}
    virtual ~GenericFD(void) {} /* = 0 */

    // All sub-classes must provide these, plus a type-specific Open call.
    virtual DIError Read(void* buf, size_t length,
        size_t* pActual = NULL) = 0;
    virtual DIError Write(const void* buf, size_t length,
        size_t* pActual = NULL) = 0;
    virtual DIError Seek(di_off_t offset, DIWhence whence) = 0;
    virtual di_off_t Tell(void) = 0;
    virtual DIError Truncate(void) = 0;
    virtual DIError Close(void) = 0;
    virtual const char* GetPathName(void) const = 0;

    // Flush-data call, only needed for physical devices
    virtual DIError Flush(void) { return kDIErrNone; }

    // Utility functions.
    virtual DIError Rewind(void) { return Seek(0, kSeekSet); }

    virtual bool GetReadOnly(void) const { return fReadOnly; }

    /*
    typedef enum {
        kGFDTypeUnknown = 0,
        kGFDTypeFile,
        kGFDTypeBuffer,
        kGFDTypeWinVolume,
        kGFDTypeGFD
    } GFDType;
    virtual GFDType GetGFDType(void) const = 0;
    */

    /*
     * Utility function to copy data from one GFD to another.  Both GFDs must
     * be seeked to their initial positions.  "length" bytes will be copied.
     */
    static DIError CopyFile(GenericFD* pDst, GenericFD* pSrc, di_off_t length,
            uint32_t* pCRC = NULL);

protected:
    GenericFD& operator=(const GenericFD&);
    GenericFD(const GenericFD&);

    bool        fReadOnly;      // set when file is opened
};

class GFDFile : public GenericFD {
public:
#ifdef HAVE_FSEEKO
    GFDFile(void) : fPathName(NULL), fFp(NULL) {}
#else
    GFDFile(void) : fPathName(NULL), fFd(-1) {}
#endif
    virtual ~GFDFile(void) { Close(); delete[] fPathName; }

    virtual DIError Open(const char* filename, bool readOnly);
    virtual DIError Read(void* buf, size_t length,
        size_t* pActual = NULL);
    virtual DIError Write(const void* buf, size_t length,
        size_t* pActual = NULL);
    virtual DIError Seek(di_off_t offset, DIWhence whence);
    virtual di_off_t Tell(void);
    virtual DIError Truncate(void);
    virtual DIError Close(void);
    virtual const char* GetPathName(void) const { return fPathName; }

private:
    char*       fPathName;

#ifdef HAVE_FSEEKO
    FILE*       fFp;
#else
    int         fFd;
#endif
};

#ifdef _WIN32
class GFDWinVolume : public GenericFD {
public:
    GFDWinVolume(void) : fPathName(NULL), fCurrentOffset(0), fVolumeEOF(-1)
        {}
    virtual ~GFDWinVolume(void) { delete[] fPathName; }

    virtual DIError Open(const char* deviceName, bool readOnly);
    virtual DIError Read(void* buf, size_t length,
        size_t* pActual = NULL);
    virtual DIError Write(const void* buf, size_t length,
        size_t* pActual = NULL);
    virtual DIError Seek(di_off_t offset, DIWhence whence);
    virtual di_off_t Tell(void);
    virtual DIError Truncate(void) { return kDIErrNotSupported; }
    virtual DIError Close(void);
    virtual const char* GetPathName(void) const { return fPathName; }

    virtual DIError Flush(void) { return fVolAccess.FlushCache(false); }

private:
    char*       fPathName;      // for display only
    Win32VolumeAccess   fVolAccess;
    di_off_t    fCurrentOffset;
    di_off_t    fVolumeEOF;
    int         fBlockSize;     // usually 512
};
#endif

class GFDBuffer : public GenericFD {
public:
    GFDBuffer(void) : fBuffer(NULL) {}
    virtual ~GFDBuffer(void) { Close(); }

    // If "doDelete" is set, the buffer will be freed with delete[] when
    // Close is called.  This should ONLY be used for storage allocated
    // by the DiskImg library, as under Windows it can cause problems
    // because DLLs can have their own heap.
    //
    // "doExpand" will cause writing past the end of the buffer to
    // reallocate the buffer.  Again, for internally-allocated storage
    // only.  We expect the initial size to be close to accurate, so we
    // don't aggressively expand the buffer.
    virtual DIError Open(void* buffer, di_off_t length, bool doDelete,
        bool doExpand, bool readOnly);
    virtual DIError Read(void* buf, size_t length,
        size_t* pActual = NULL);
    virtual DIError Write(const void* buf, size_t length,
        size_t* pActual = NULL);
    virtual DIError Seek(di_off_t offset, DIWhence whence);
    virtual di_off_t Tell(void);
    virtual DIError Truncate(void) {
        fLength = (long) Tell();
        return kDIErrNone;
    }
    virtual DIError Close(void);
    virtual const char* GetPathName(void) const { return NULL; }

    // Back door; try not to use this.
    void* GetBuffer(void) const { return fBuffer; }

private:
    enum { kMaxReasonableSize = 256 * 1024 * 1024 };
    void*       fBuffer;
    long        fLength;        // these sit in memory, so there's no
    long        fAllocLength;   //  value in using di_off_t here
    bool        fDoDelete;
    bool        fDoExpand;
    di_off_t    fCurrentOffset; // actually limited to (long)
};

#if 0
class GFDEmbedded : public GenericFD {
public:
    GFDEmbedded(void) : fEFD(NULL) {}
    virtual ~GFDEmbedded(void) { Close(); }

    virtual DIError Open(EmbeddedFD* pEFD, bool readOnly);
    virtual DIError Read(void* buf, size_t length,
        size_t* pActual = NULL);
    virtual DIError Write(const void* buf, size_t length,
        size_t* pActual = NULL);
    virtual DIError Seek(di_off_t offset, DIWhence whence);
    virtual di_off_t Tell(void);
    virtual DIError Close(void);
    virtual const char* GetPathName(void) const { return NULL; }

private:
    EmbeddedFD* fEFD;
};
#endif

/* pass all requests straight through to another GFD (with offset bias) */
class GFDGFD : public GenericFD {
public:
    GFDGFD(void) : fpGFD(NULL), fOffset(0) {}
    virtual ~GFDGFD(void) { Close(); }

    virtual DIError Open(GenericFD* pGFD, di_off_t offset, bool readOnly) {
        if (pGFD == NULL)
            return kDIErrInvalidArg;
        if (!readOnly && pGFD->GetReadOnly())
            return kDIErrAccessDenied;          // can't convert to read-write
        fpGFD = pGFD;
        fOffset = offset;
        fReadOnly = readOnly;
        Seek(0, kSeekSet);
        return kDIErrNone;
    }
    virtual DIError Read(void* buf, size_t length,
        size_t* pActual = NULL)
    {
        return fpGFD->Read(buf, length, pActual);
    }
    virtual DIError Write(const void* buf, size_t length,
        size_t* pActual = NULL)
    {
        return fpGFD->Write(buf, length, pActual);
    }
    virtual DIError Seek(di_off_t offset, DIWhence whence) {
        return fpGFD->Seek(offset + fOffset, whence);
    }
    virtual di_off_t Tell(void) {
        return fpGFD->Tell() -fOffset;
    }
    virtual DIError Truncate(void) {
        return fpGFD->Truncate();
    }
    virtual DIError Close(void) {
        /* do NOT close underlying descriptor */
        fpGFD = NULL;
        return kDIErrNone;
    }
    virtual const char* GetPathName(void) const { return fpGFD->GetPathName(); }

private:
    GenericFD*  fpGFD;
    di_off_t    fOffset;
};

};  // namespace DiskImgLib

#endif /*__GENERIC_FD__*/
