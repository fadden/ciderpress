/*
 * NuFX archive manipulation library
 * Copyright (C) 2000-2007 by Andy McFadden, All Rights Reserved.
 * This is free software; you can redistribute it and/or modify it under the
 * terms of the BSD License, see the file COPYING-LIB.
 *
 * External interface (types, defines, and function prototypes).
 */
#ifndef __NufxLib__
#define __NufxLib__

#include <stdio.h>


#ifdef __cplusplus
extern "C" {
#endif

/*
 * NufxLib version number.  Compare these values (which represent the
 * version against which your application was compiled) to the values
 * returned by NuGetVersion (representing the version against which
 * your application is statically or dynamically linked).  If the major
 * number doesn't match exactly, an existing interface has changed and you
 * should halt immediately.  If the minor number from NuGetVersion is
 * less, there may be new interfaces, new features, or bug fixes missing
 * upon which your application depends, so you should halt immediately.
 * (If the minor number is greater, there are new features, but your
 * application will not be affected by them.)
 *
 * The "bug" version can usually be ignored, since it represents minor
 * fixes.  Unless, of course, your code depends upon that fix.
 */
#define kNuVersionMajor     2
#define kNuVersionMinor     2
#define kNuVersionBug       2


/*
 * ===========================================================================
 *      Types
 * ===========================================================================
 */

/*
 * Error values returned from functions.
 *
 * These are negative so that they don't conflict with system-defined
 * errors (like ENOENT).  A NuError can hold either.
 */
typedef enum NuError {
    kNuErrNone          = 0,

    kNuErrGeneric       = -1,
    kNuErrInternal      = -2,
    kNuErrUsage         = -3,
    kNuErrSyntax        = -4,
    kNuErrMalloc        = -5,
    kNuErrInvalidArg    = -6,
    kNuErrBadStruct     = -7,
    kNuErrUnexpectedNil = -8,
    kNuErrBusy          = -9,

    kNuErrSkipped       = -10,      /* processing skipped by request */
    kNuErrAborted       = -11,      /* processing aborted by request */
    kNuErrRename        = -12,      /* user wants to rename before extracting */

    kNuErrFile          = -20,
    kNuErrFileOpen      = -21,
    kNuErrFileClose     = -22,
    kNuErrFileRead      = -23,
    kNuErrFileWrite     = -24,
    kNuErrFileSeek      = -25,
    kNuErrFileExists    = -26,      /* existed when it shouldn't */
    kNuErrFileNotFound  = -27,      /* didn't exist when it should have */
    kNuErrFileStat      = -28,      /* some sort of GetFileInfo failure */
    kNuErrFileNotReadable = -29,    /* bad access permissions */

    kNuErrDirExists     = -30,      /* dir exists, don't need to create it */
    kNuErrNotDir        = -31,      /* expected a dir, got a regular file */
    kNuErrNotRegularFile = -32,     /* expected regular file, got weirdness */
    kNuErrDirCreate     = -33,      /* unable to create a directory */
    kNuErrOpenDir       = -34,      /* error opening directory */
    kNuErrReadDir       = -35,      /* error reading directory */
    kNuErrFileSetDate   = -36,      /* unable to set file date */
    kNuErrFileSetAccess = -37,      /* unable to set file access permissions */
    kNuErrFileAccessDenied = -38,   /* equivalent to EACCES */

    kNuErrNotNuFX       = -40,      /* 'NuFile' missing; not a NuFX archive? */
    kNuErrBadMHVersion  = -41,      /* bad master header version */
    kNuErrRecHdrNotFound = -42,     /* 'NuFX' missing; corrupted archive? */
    kNuErrNoRecords     = -43,      /* archive doesn't have any records */
    kNuErrBadRecord     = -44,      /* something about the record looked bad */
    kNuErrBadMHCRC      = -45,      /* bad master header CRC */
    kNuErrBadRHCRC      = -46,      /* bad record header CRC */
    kNuErrBadThreadCRC  = -47,      /* bad thread header CRC */
    kNuErrBadDataCRC    = -48,      /* bad CRC detected in the data */

    kNuErrBadFormat     = -50,      /* compression type not supported */
    kNuErrBadData       = -51,      /* expansion func didn't like input */
    kNuErrBufferOverrun = -52,      /* overflowed a user buffer */
    kNuErrBufferUnderrun = -53,     /* underflowed a user buffer */
    kNuErrOutMax        = -54,      /* output limit exceeded */

    kNuErrNotFound      = -60,      /* (generic) search unsuccessful */
    kNuErrRecordNotFound = -61,     /* search for specific record failed */
    kNuErrRecIdxNotFound = -62,     /* search by NuRecordIdx failed */
    kNuErrThreadIdxNotFound = -63,  /* search by NuThreadIdx failed */
    kNuErrThreadIDNotFound = -64,   /* search by NuThreadID failed */
    kNuErrRecNameNotFound = -65,    /* search by storageName failed */
    kNuErrRecordExists  = -66,      /* found existing record with same name */

    kNuErrAllDeleted    = -70,      /* attempt to delete everything */
    kNuErrArchiveRO     = -71,      /* archive is open in read-only mode */
    kNuErrModRecChange  = -72,      /* tried to change modified record */
    kNuErrModThreadChange = -73,    /* tried to change modified thread */
    kNuErrThreadAdd     = -74,      /* adding that thread creates a conflict */
    kNuErrNotPreSized   = -75,      /* tried to update a non-pre-sized thread */
    kNuErrPreSizeOverflow = -76,    /* too much data */
    kNuErrInvalidFilename = -77,    /* invalid filename */

    kNuErrLeadingFssep  = -80,      /* names in archives must not start w/sep */
    kNuErrNotNewer      = -81,      /* item same age or older than existing */
    kNuErrDuplicateNotFound = -82,  /* "must overwrite" was set, but item DNE */
    kNuErrDamaged       = -83,      /* original archive may have been damaged */

    kNuErrIsBinary2     = -90,      /* this looks like a Binary II archive */

    kNuErrUnknownFeature =-100,     /* attempt to test unknown feature */
    kNuErrUnsupFeature  = -101,     /* feature not supported */
} NuError;

/*
 * Return values from callback functions.
 */
typedef enum NuResult {
    kNuOK               = 0,
    kNuSkip             = 1,
    kNuAbort            = 2,
    /*kNuAbortAll       = 3,*/
    kNuRetry            = 4,
    kNuIgnore           = 5,
    kNuRename           = 6,
    kNuOverwrite        = 7
} NuResult;

/*
 * NuRecordIdxs are assigned to records in an archive.  You may assume that
 * the values are unique, but that is all.
 */
typedef unsigned long NuRecordIdx;

/*
 * NuThreadIdxs are assigned to threads within a record.  Again, you may
 * assume that the values are unique within a record, but that is all.
 */
typedef unsigned long NuThreadIdx;

/*
 * Thread ID, a combination of thread_class and thread_kind.  Standard
 * values have explicit identifiers.
 */
typedef unsigned long NuThreadID;
#define NuMakeThreadID(class, kind) /* construct a NuThreadID */ \
            ((unsigned long)(class) << 16 | (unsigned long)(kind))
#define NuGetThreadID(pThread)      /* pull NuThreadID out of NuThread */ \
            (NuMakeThreadID((pThread)->thThreadClass, (pThread)->thThreadKind))
#define NuThreadIDGetClass(threadID) /* get threadClass from NuThreadID */ \
            ((unsigned short) ((unsigned long)(threadID) >> 16))
#define NuThreadIDGetKind(threadID) /* get threadKind from NuThreadID */ \
            ((unsigned short) ((threadID) & 0xffff))
#define kNuThreadClassMessage   0x0000
#define kNuThreadClassControl   0x0001
#define kNuThreadClassData      0x0002
#define kNuThreadClassFilename  0x0003
#define kNuThreadIDOldComment   NuMakeThreadID(kNuThreadClassMessage, 0x0000)
#define kNuThreadIDComment      NuMakeThreadID(kNuThreadClassMessage, 0x0001)
#define kNuThreadIDIcon         NuMakeThreadID(kNuThreadClassMessage, 0x0002)
#define kNuThreadIDMkdir        NuMakeThreadID(kNuThreadClassControl, 0x0000)
#define kNuThreadIDDataFork     NuMakeThreadID(kNuThreadClassData, 0x0000)
#define kNuThreadIDDiskImage    NuMakeThreadID(kNuThreadClassData, 0x0001)
#define kNuThreadIDRsrcFork     NuMakeThreadID(kNuThreadClassData, 0x0002)
#define kNuThreadIDFilename     NuMakeThreadID(kNuThreadClassFilename, 0x0000)
#define kNuThreadIDWildcard     NuMakeThreadID(0xffff, 0xffff)

/* enumerate the possible values for thThreadFormat */
typedef enum NuThreadFormat {
    kNuThreadFormatUncompressed = 0x0000,
    kNuThreadFormatHuffmanSQ    = 0x0001,
    kNuThreadFormatLZW1         = 0x0002,
    kNuThreadFormatLZW2         = 0x0003,
    kNuThreadFormatLZC12        = 0x0004,
    kNuThreadFormatLZC16        = 0x0005,
    kNuThreadFormatDeflate      = 0x0006,   /* NOTE: not in NuFX standard */
    kNuThreadFormatBzip2        = 0x0007,   /* NOTE: not in NuFX standard */
} NuThreadFormat;


/* extract the filesystem separator char from the "file_sys_info" field */
#define NuGetSepFromSysInfo(sysInfo) \
            ((char) ((sysInfo) & 0xff))
/* return a file_sys_info with a replaced filesystem separator */
#define NuSetSepInSysInfo(sysInfo, newSep) \
            ((unsigned short) (((sysInfo) & 0xff00) | ((newSep) & 0xff)) )

/* GS/OS-defined file system identifiers; sadly, UNIX is not among them */
typedef enum NuFileSysID {
    kNuFileSysUnknown           = 0,    /* NuFX spec says use this */
    kNuFileSysProDOS            = 1,
    kNuFileSysDOS33             = 2,
    kNuFileSysDOS32             = 3,
    kNuFileSysPascal            = 4,
    kNuFileSysMacHFS            = 5,
    kNuFileSysMacMFS            = 6,
    kNuFileSysLisa              = 7,
    kNuFileSysCPM               = 8,
    kNuFileSysCharFST           = 9,
    kNuFileSysMSDOS             = 10,
    kNuFileSysHighSierra        = 11,
    kNuFileSysISO9660           = 12,
    kNuFileSysAppleShare        = 13
} NuFileSysID;

/* simplified definition of storage types */
typedef enum NuStorageType {
    kNuStorageUnknown           = 0,    /* (used by ProDOS for deleted files) */
    kNuStorageSeedling          = 1,    /* <= 512 bytes */
    kNuStorageSapling           = 2,    /* < 128KB */
    kNuStorageTree              = 3,    /* < 16MB */
    kNuStoragePascalVol         = 4,    /* (embedded pascal volume; rare) */
    kNuStorageExtended          = 5,    /* forked (any size) */
    kNuStorageDirectory         = 13,   /* directory */
    kNuStorageSubdirHeader      = 14,   /* (only used in subdir headers) */
    kNuStorageVolumeHeader      = 15,   /* (only used in volume dir header) */
} NuStorageType;

/* bit flags for NuOpenRW */
enum {
    kNuOpenCreat                = 0x0001,
    kNuOpenExcl                 = 0x0002
};


/*
 * The actual NuArchive structure is opaque, and should only be visible
 * to the library.  We define it here as an ambiguous struct.
 */
typedef struct NuArchive NuArchive;

/*
 * Generic callback prototype.
 */
typedef NuResult (*NuCallback)(NuArchive* pArchive, void* args);

/*
 * Parameters that affect archive operations.
 */
typedef enum NuValueID {
    kNuValueInvalid             = 0,
    kNuValueIgnoreCRC           = 1,
    kNuValueDataCompression     = 2,
    kNuValueDiscardWrapper      = 3,
    kNuValueEOL                 = 4,
    kNuValueConvertExtractedEOL = 5,
    kNuValueOnlyUpdateOlder     = 6,
    kNuValueAllowDuplicates     = 7,
    kNuValueHandleExisting      = 8,
    kNuValueModifyOrig          = 9,
    kNuValueMimicSHK            = 10,
    kNuValueMaskDataless        = 11,
    kNuValueStripHighASCII      = 12,
    kNuValueJunkSkipMax         = 13,
    kNuValueIgnoreLZW2Len       = 14,
    kNuValueHandleBadMac        = 15
} NuValueID;
typedef unsigned long NuValue;

/*
 * Enumerated values for things you pass in a NuValue.
 */
enum NuValueValue {
    /* for the truly retentive */
    kNuValueFalse               = 0,
    kNuValueTrue                = 1,

    /* for kNuValueDataCompression */
    kNuCompressNone             = 10,
    kNuCompressSQ               = 11,
    kNuCompressLZW1             = 12,
    kNuCompressLZW2             = 13,
    kNuCompressLZC12            = 14,
    kNuCompressLZC16            = 15,
    kNuCompressDeflate          = 16,
    kNuCompressBzip2            = 17,

    /* for kNuValueEOL */
    kNuEOLUnknown               = 50,
    kNuEOLCR                    = 51,
    kNuEOLLF                    = 52,
    kNuEOLCRLF                  = 53,

    /* for kNuValueConvertExtractedEOL */
    kNuConvertOff               = 60,
    kNuConvertOn                = 61,
    kNuConvertAuto              = 62,

    /* for kNuValueHandleExisting */
    kNuMaybeOverwrite           = 90,
    kNuNeverOverwrite           = 91,
    kNuAlwaysOverwrite          = 93,
    kNuMustOverwrite            = 94
};


/*
 * Pull out archive attributes.
 */
typedef enum NuAttrID {
    kNuAttrInvalid          = 0,
    kNuAttrArchiveType      = 1,
    kNuAttrNumRecords       = 2,
    kNuAttrHeaderOffset     = 3,
    kNuAttrJunkOffset       = 4,
} NuAttrID;
typedef unsigned long NuAttr;

/*
 * Archive types.
 */
typedef enum NuArchiveType {
    kNuArchiveUnknown,          /* .??? */
    kNuArchiveNuFX,             /* .SHK (sometimes .SDK) */
    kNuArchiveNuFXInBNY,        /* .BXY */
    kNuArchiveNuFXSelfEx,       /* .SEA */
    kNuArchiveNuFXSelfExInBNY,  /* .BSE */

    kNuArchiveBNY               /* .BNY, .BQY - not supported */
} NuArchiveType;


/*
 * Some common values for "locked" and "unlocked".  Under ProDOS each bit
 * can be set independently, so don't use these defines to *interpret*
 * what you see.  They're reasonable things to *set* the access field to.
 *
 * The defined bits are:
 *  0x80 'D' destroy enabled
 *  0x40 'N' rename enabled
 *  0x20 'B' file needs to be backed up
 *  0x10 (reserved, must be zero)
 *  0x08 (reserved, must be zero)
 *  0x04 'I' file is invisible
 *  0x02 'W' write enabled
 *  0x01 'R' read enabled
 */
#define kNuAccessLocked     0x21
#define kNuAccessUnlocked   0xe3


/*
 * NuFlush result flags.
 */
#define kNuFlushSucceeded       (1L)
#define kNuFlushAborted         (1L << 1)
#define kNuFlushCorrupted       (1L << 2)
#define kNuFlushReadOnly        (1L << 3)
#define kNuFlushInaccessible    (1L << 4)


/*
 * ===========================================================================
 *      NuFX archive defintions
 * ===========================================================================
 */

typedef struct NuThreadMod NuThreadMod;     /* dummy def for internal struct */
typedef union NuDataSource NuDataSource;    /* dummy def for internal struct */
typedef union NuDataSink NuDataSink;        /* dummy def for internal struct */

/*
 * NuFX Date/Time structure; same as TimeRec from IIgs "misctool.h".
 */
typedef struct NuDateTime {
    unsigned char   second;         /* 0-59 */
    unsigned char   minute;         /* 0-59 */
    unsigned char   hour;           /* 0-23 */
    unsigned char   year;           /* year - 1900 */
    unsigned char   day;            /* 0-30 */
    unsigned char   month;          /* 0-11 */
    unsigned char   extra;          /* (must be zero) */
    unsigned char   weekDay;        /* 1-7 (1=sunday) */
} NuDateTime;

/*
 * NuFX "thread" definition.
 *
 * Guaranteed not to have pointers in it.  Can be copied with memcpy or
 * assignment.
 */
typedef struct NuThread {
    /* from the archive */
    unsigned short      thThreadClass;
    NuThreadFormat      thThreadFormat;
    unsigned short      thThreadKind;
    unsigned short      thThreadCRC;    /* comp or uncomp data; see rec vers */
    unsigned long       thThreadEOF;
    unsigned long       thCompThreadEOF;

    /* extra goodies */
    NuThreadIdx         threadIdx;
    unsigned long       actualThreadEOF;    /* disk images might be off */
    long                fileOffset;         /* fseek offset to data in shk */

    /* internal use only */
    unsigned short      used;               /* mark as uninteresting */
} NuThread;

/*
 * NuFX "record" definition.
 *
 * (Note to developers: update Nu_AddRecord if this changes.)
 */
#define kNufxIDLen                  4       /* len of 'NuFX' with funky MSBs */
#define kNuReasonableAttribCount    256
#define kNuReasonableFilenameLen    1024
#define kNuReasonableTotalThreads   16
#define kNuMaxRecordVersion         3       /* max we can handle */
#define kNuOurRecordVersion         3       /* what we write */
typedef struct NuRecord {
    /* version 0+ */
    unsigned char       recNufxID[kNufxIDLen];
    unsigned short      recHeaderCRC;
    unsigned short      recAttribCount;
    unsigned short      recVersionNumber;
    unsigned long       recTotalThreads;
    NuFileSysID         recFileSysID;
    unsigned short      recFileSysInfo;
    unsigned long       recAccess;
    unsigned long       recFileType;
    unsigned long       recExtraType;
    unsigned short      recStorageType;     /* NuStorage*,file_sys_block_size */
    NuDateTime          recCreateWhen;
    NuDateTime          recModWhen;
    NuDateTime          recArchiveWhen;

    /* option lists only in version 1+ */
    unsigned short      recOptionSize;
    unsigned char*      recOptionList;      /* NULL if v0 or recOptionSize==0 */

    /* data specified by recAttribCount, not accounted for by option list */
    long                extraCount;
    unsigned char*      extraBytes;

    unsigned short      recFilenameLength;  /* usually zero */
    char*               recFilename;        /* doubles as disk volume_name */

    /* extra goodies; "dirtyHeader" does not apply to anything below */
    NuRecordIdx         recordIdx;      /* session-unique record index */
    char*               threadFilename; /* extracted from filename thread */
    char*               newFilename;    /* memorized during "add file" call */
    const char*         filename;       /* points at recFilen or threadFilen */
    unsigned long       recHeaderLength; /* size of rec hdr, incl thread hdrs */
    unsigned long       totalCompLength; /* total len of data in archive file */
    long                fakeThreads;    /* used by "MaskDataless" */
    int                 isBadMac;       /* malformed "bad mac" header */

    long                fileOffset;     /* file offset of record header */

    /* use provided interface to access this */
    struct NuThread*    pThreads;       /* ptr to thread array */

    /* private -- things the application shouldn't look at */
    struct NuRecord*    pNext;          /* used internally */
    NuThreadMod*        pThreadMods;    /* used internally */
    short               dirtyHeader;    /* set in "copy" when hdr fields uptd */
    short               dropRecFilename; /* if set, we're dropping this name */
} NuRecord;

/*
 * NuFX "master header" definition.
 *
 * The "mhReserved2" entry doesn't appear in my copy of the $e0/8002 File
 * Type Note, but as best as I can recall the MH block must be 48 bytes.
 */
#define kNufileIDLen                6   /* length of 'NuFile' with funky MSBs */
#define kNufileMasterReserved1Len   8
#define kNufileMasterReserved2Len   6
#define kNuMaxMHVersion             2       /* max we can handle */
#define kNuOurMHVersion             2       /* what we write */
typedef struct NuMasterHeader {
    unsigned char       mhNufileID[kNufileIDLen];
    unsigned short      mhMasterCRC;
    unsigned long       mhTotalRecords;
    NuDateTime          mhArchiveCreateWhen;
    NuDateTime          mhArchiveModWhen;
    unsigned short      mhMasterVersion;
    unsigned char       mhReserved1[kNufileMasterReserved1Len];
    unsigned long       mhMasterEOF;
    unsigned char       mhReserved2[kNufileMasterReserved2Len];

    /* private -- internal use only */
    short               isValid;
} NuMasterHeader;


/*
 * ===========================================================================
 *      Misc declarations
 * ===========================================================================
 */

/*
 * Record attributes that can be changed with NuSetRecordAttr.  This is
 * a small subset of the full record.
 */
typedef struct NuRecordAttr {
    NuFileSysID         fileSysID;
    /*unsigned short        fileSysInfo;*/
    unsigned long       access;
    unsigned long       fileType;
    unsigned long       extraType;
    NuDateTime          createWhen;
    NuDateTime          modWhen;
    NuDateTime          archiveWhen;
} NuRecordAttr;

/*
 * Some additional details about a file.
 */
typedef struct NuFileDetails {
    /* used during AddFile call */
    NuThreadID          threadID;       /* data, rsrc, disk img? */
    const char*         origName;

    /* these go straight into the NuRecord */
    const char*         storageName;
    NuFileSysID         fileSysID;
    unsigned short      fileSysInfo;
    unsigned long       access;
    unsigned long       fileType;
    unsigned long       extraType;
    unsigned short      storageType;    /* use Unknown, or disk block size */
    NuDateTime          createWhen;
    NuDateTime          modWhen;
    NuDateTime          archiveWhen;
} NuFileDetails;


/*
 * Passed into the SelectionFilter callback.
 */
typedef struct NuSelectionProposal {
    const NuRecord*     pRecord;
    const NuThread*     pThread;
} NuSelectionProposal;

/*
 * Passed into the OutputPathnameFilter callback.
 */
typedef struct NuPathnameProposal {
    const char*         pathname;
    char                filenameSeparator;
    const NuRecord*     pRecord;
    const NuThread*     pThread;

    const char*         newPathname;
    unsigned char       newFilenameSeparator;
    /*NuThreadID        newStorage;*/
    NuDataSink*         newDataSink;
} NuPathnameProposal;


/* used by error handler and progress updater to indicate what we're doing */
typedef enum NuOperation {
    kNuOpUnknown = 0,
    kNuOpAdd,
    kNuOpExtract,
    kNuOpTest,
    kNuOpDelete,        /* not used for progress updates */
    kNuOpContents       /* not used for progress updates */
} NuOperation;

/* state of progress when adding or extracting */
typedef enum NuProgressState {
    kNuProgressPreparing,       /* not started yet */
    kNuProgressOpening,         /* opening files */

    kNuProgressAnalyzing,       /* analyzing data */
    kNuProgressCompressing,     /* compressing data */
    kNuProgressStoring,         /* storing (no compression) data */
    kNuProgressExpanding,       /* expanding data */
    kNuProgressCopying,         /* copying data (in or out) */

    kNuProgressDone,            /* all done, success */
    kNuProgressSkipped,         /* all done, we skipped this one */
    kNuProgressAborted,         /* all done, user cancelled the operation */
    kNuProgressFailed           /* all done, failure */
} NuProgressState;

/*
 * Passed into the ProgressUpdater callback.
 *
 * [ Thought for the day: add an optional flag that causes us to only
 *   call the progressFunc when the "percentComplete" changes by more
 *   than a specified amount. ]
 */
typedef struct NuProgressData {
    /* what are we doing */
    NuOperation         operation;
    /* what specifically are we doing */
    NuProgressState     state;
    /* how far along are we */
    short               percentComplete;    /* 0-100 */

    /* original pathname (in archive for expand, on disk for compress) */
    const char*         origPathname;
    /* processed pathname (PathnameFilter for expand, in-record for compress) */
    const char*         pathname;
    /* basename of "pathname" */
    const char*         filename;
    /* pointer to the record we're expanding from */
    const NuRecord*     pRecord;

    unsigned long       uncompressedLength; /* size of uncompressed data */
    unsigned long       uncompressedProgress;   /* #of bytes in/out */

    struct {
        NuThreadFormat      threadFormat;       /* compression being applied */
    } compress;

    struct {
        unsigned long       totalCompressedLength;  /* all "data" threads */
        unsigned long       totalUncompressedLength;

        /*unsigned long     compressedLength;    * size of compressed data */
        /*unsigned long     compressedProgress;  * #of compressed bytes in/out*/
        const NuThread*     pThread;            /* thread we're working on */
        NuValue             convertEOL;         /* set if LF/CR conv is on */
    } expand;

    /* pay no attention */
    NuCallback          progressFunc;
} NuProgressData;

/*
 * Passed into the ErrorHandler callback.
 */
typedef struct NuErrorStatus {
    NuOperation         operation;      /* were we adding, extracting, ?? */
    NuError             err;            /* library error code */
    int                 sysErr;         /* system error code, if applicable */
    const char*         message;        /* (optional) message to user */
    const NuRecord*     pRecord;        /* relevant record, if any */
    const char*         pathname;       /* problematic pathname, if any */
    const char*         origPathname;   /* original pathname, if any */
    char                filenameSeparator;  /* fssep for pathname, if any */
    /*char              origArchiveTouched;*/

    char                canAbort;       /* give option to abort */
    /*char              canAbortAll;*/  /* abort + discard all recent changes */
    char                canRetry;       /* give option to retry same op */
    char                canIgnore;      /* give option to ignore error */
    char                canSkip;        /* give option to skip this file/rec */
    char                canRename;      /* give option to rename file */
    char                canOverwrite;   /* give option to overwrite file */
} NuErrorStatus;

/*
 * Error message callback gets one of these.
 */
typedef struct NuErrorMessage {
    const char*         message;        /* the message itself */
    NuError             err;            /* relevant error code (may be none) */
    short               isDebug;        /* set for debug-only messages */

    /* these identify where the message originated if lib built w/debug set */
    const char*         file;           /* source file */
    int                 line;           /* line number */
    const char*         function;       /* function name (might be nil) */
} NuErrorMessage;


/*
 * Options for the NuTestFeature function.
 */
typedef enum NuFeature {
    kNuFeatureUnknown = 0,

    kNuFeatureCompressSQ = 1,           /* kNuThreadFormatHuffmanSQ */
    kNuFeatureCompressLZW = 2,          /* kNuThreadFormatLZW1 and LZW2 */
    kNuFeatureCompressLZC = 3,          /* kNuThreadFormatLZC12 and LZC16 */
    kNuFeatureCompressDeflate = 4,      /* kNuThreadFormatDeflate */
    kNuFeatureCompressBzip2 = 5,        /* kNuThreadFormatBzip2 */
} NuFeature;


/*
 * ===========================================================================
 *      Function prototypes
 * ===========================================================================
 */

/*
 * Win32 dll magic.
 */
#if defined(_WIN32)
# include <windows.h>
# if defined(NUFXLIB_EXPORTS)
   /* building the NufxLib DLL */
#  define NUFXLIB_API __declspec(dllexport)
# elif defined (NUFXLIB_DLL)
   /* building to link against the NufxLib DLL */
#  define NUFXLIB_API __declspec(dllimport)
# else
   /* using static libs */
#  define NUFXLIB_API
# endif
#else
  /* not using Win32... hooray! */
# define NUFXLIB_API
#endif

/* streaming and non-streaming read-only interfaces */
NUFXLIB_API NuError NuStreamOpenRO(FILE* infp, NuArchive** ppArchive);
NUFXLIB_API NuError NuContents(NuArchive* pArchive, NuCallback contentFunc);
NUFXLIB_API NuError NuExtract(NuArchive* pArchive);
NUFXLIB_API NuError NuTest(NuArchive* pArchive);

/* strictly non-streaming read-only interfaces */
NUFXLIB_API NuError NuOpenRO(const char* archivePathname,NuArchive** ppArchive);
NUFXLIB_API NuError NuExtractRecord(NuArchive* pArchive, NuRecordIdx recordIdx);
NUFXLIB_API NuError NuExtractThread(NuArchive* pArchive, NuThreadIdx threadIdx,
            NuDataSink* pDataSink);
NUFXLIB_API NuError NuTestRecord(NuArchive* pArchive, NuRecordIdx recordIdx);
NUFXLIB_API NuError NuGetRecord(NuArchive* pArchive, NuRecordIdx recordIdx,
            const NuRecord** ppRecord);
NUFXLIB_API NuError NuGetRecordIdxByName(NuArchive* pArchive, const char* name,
            NuRecordIdx* pRecordIdx);
NUFXLIB_API NuError NuGetRecordIdxByPosition(NuArchive* pArchive,
            unsigned long position, NuRecordIdx* pRecordIdx);

/* read/write interfaces */
NUFXLIB_API NuError NuOpenRW(const char* archivePathname,
            const char* tempPathname, unsigned long flags,
            NuArchive** ppArchive);
NUFXLIB_API NuError NuFlush(NuArchive* pArchive, long* pStatusFlags);
NUFXLIB_API NuError NuAddRecord(NuArchive* pArchive,
            const NuFileDetails* pFileDetails, NuRecordIdx* pRecordIdx);
NUFXLIB_API NuError NuAddThread(NuArchive* pArchive, NuRecordIdx recordIdx,
            NuThreadID threadID, NuDataSource* pDataSource,
            NuThreadIdx* pThreadIdx);
NUFXLIB_API NuError NuAddFile(NuArchive* pArchive, const char* pathname,
            const NuFileDetails* pFileDetails, short fromRsrcFork,
            NuRecordIdx* pRecordIdx);
NUFXLIB_API NuError NuRename(NuArchive* pArchive, NuRecordIdx recordIdx,
            const char* pathname, char fssep);
NUFXLIB_API NuError NuSetRecordAttr(NuArchive* pArchive, NuRecordIdx recordIdx,
            const NuRecordAttr* pRecordAttr);
NUFXLIB_API NuError NuUpdatePresizedThread(NuArchive* pArchive,
            NuThreadIdx threadIdx, NuDataSource* pDataSource, long* pMaxLen);
NUFXLIB_API NuError NuDelete(NuArchive* pArchive);
NUFXLIB_API NuError NuDeleteRecord(NuArchive* pArchive, NuRecordIdx recordIdx);
NUFXLIB_API NuError NuDeleteThread(NuArchive* pArchive, NuThreadIdx threadIdx);

/* general interfaces */
NUFXLIB_API NuError NuClose(NuArchive* pArchive);
NUFXLIB_API NuError NuAbort(NuArchive* pArchive);
NUFXLIB_API NuError NuGetMasterHeader(NuArchive* pArchive,
            const NuMasterHeader** ppMasterHeader);
NUFXLIB_API NuError NuGetExtraData(NuArchive* pArchive, void** ppData);
NUFXLIB_API NuError NuSetExtraData(NuArchive* pArchive, void* pData);
NUFXLIB_API NuError NuGetValue(NuArchive* pArchive, NuValueID ident,
            NuValue* pValue);
NUFXLIB_API NuError NuSetValue(NuArchive* pArchive, NuValueID ident,
            NuValue value);
NUFXLIB_API NuError NuGetAttr(NuArchive* pArchive, NuAttrID ident,
            NuAttr* pAttr);
NUFXLIB_API NuError NuDebugDumpArchive(NuArchive* pArchive);

/* sources and sinks */
NUFXLIB_API NuError NuCreateDataSourceForFile(NuThreadFormat threadFormat,
            unsigned long otherLen, const char* pathname,
            short isFromRsrcFork, NuDataSource** ppDataSource);
NUFXLIB_API NuError NuCreateDataSourceForFP(NuThreadFormat threadFormat,
            unsigned long otherLen, FILE* fp, long offset, long length,
            NuCallback closeFunc, NuDataSource** ppDataSource);
NUFXLIB_API NuError NuCreateDataSourceForBuffer(NuThreadFormat threadFormat,
            unsigned long otherLen, const unsigned char* buffer, long offset,
            long length, NuCallback freeFunc, NuDataSource** ppDataSource);
NUFXLIB_API NuError NuFreeDataSource(NuDataSource* pDataSource);
NUFXLIB_API NuError NuDataSourceSetRawCrc(NuDataSource* pDataSource,
            unsigned short crc);
NUFXLIB_API NuError NuCreateDataSinkForFile(short doExpand, NuValue convertEOL,
            const char* pathname, char fssep, NuDataSink** ppDataSink);
NUFXLIB_API NuError NuCreateDataSinkForFP(short doExpand, NuValue convertEOL,
            FILE* fp, NuDataSink** ppDataSink);
NUFXLIB_API NuError NuCreateDataSinkForBuffer(short doExpand,
            NuValue convertEOL, unsigned char* buffer, unsigned long bufLen,
            NuDataSink** ppDataSink);
NUFXLIB_API NuError NuFreeDataSink(NuDataSink* pDataSink);
NUFXLIB_API NuError NuDataSinkGetOutCount(NuDataSink* pDataSink,
            unsigned long* pOutCount);

/* miscellaneous non-archive operations */
NUFXLIB_API NuError NuGetVersion(long* pMajorVersion, long* pMinorVersion,
            long* pBugVersion, const char** ppBuildDate,
            const char** ppBuildFlags);
NUFXLIB_API const char* NuStrError(NuError err);
NUFXLIB_API NuError NuTestFeature(NuFeature feature);
NUFXLIB_API void NuRecordCopyAttr(NuRecordAttr* pRecordAttr,
            const NuRecord* pRecord);
NUFXLIB_API NuError NuRecordCopyThreads(const NuRecord* pRecord,
            NuThread** ppThreads);
NUFXLIB_API unsigned long NuRecordGetNumThreads(const NuRecord* pRecord);
NUFXLIB_API const NuThread* NuThreadGetByIdx(const NuThread* pThread, long idx);
NUFXLIB_API short NuIsPresizedThreadID(NuThreadID threadID);

#define NuGetThread(pRecord, idx) ( (const NuThread*)                       \
        ((unsigned long) (idx) < (unsigned long) (pRecord)->recTotalThreads ? \
                &(pRecord)->pThreads[(idx)] : NULL)                         \
        )


/* callback setters */
#define kNuInvalidCallback  ((NuCallback) 1)
NUFXLIB_API NuCallback NuSetSelectionFilter(NuArchive* pArchive,
            NuCallback filterFunc);
NUFXLIB_API NuCallback NuSetOutputPathnameFilter(NuArchive* pArchive,
            NuCallback filterFunc);
NUFXLIB_API NuCallback NuSetProgressUpdater(NuArchive* pArchive,
            NuCallback updateFunc);
NUFXLIB_API NuCallback NuSetErrorHandler(NuArchive* pArchive,
            NuCallback errorFunc);
NUFXLIB_API NuCallback NuSetErrorMessageHandler(NuArchive* pArchive,
            NuCallback messageHandlerFunc);
NUFXLIB_API NuCallback NuSetGlobalErrorMessageHandler(NuCallback messageHandlerFunc);


#ifdef __cplusplus
}
#endif

#endif /*__NufxLib__*/
