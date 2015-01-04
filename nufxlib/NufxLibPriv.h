/*
 * NuFX archive manipulation library
 * Copyright (C) 2000-2007 by Andy McFadden, All Rights Reserved.
 * This is free software; you can redistribute it and/or modify it under the
 * terms of the BSD License, see the file COPYING-LIB.
 *
 * Global internal declarations and definitions.
 */
#ifndef NUFXLIB_NUFXLIBPRIV_H
#define NUFXLIB_NUFXLIBPRIV_H

/* include files that everybody needs */
#include "SysDefs.h"
#include "NufxLib.h"
#include "MiscStuff.h"

#ifdef USE_DMALLOC
/* enable with something like "dmalloc -l logfile -i 100 medium" */
# include "dmalloc.h"
#endif


/*
 * ===========================================================================
 *      NuArchive definition
 * ===========================================================================
 */

/*
 * Archives can be opened in streaming read-only, non-streaming read-only,
 * and non-streaming read-write mode.
 */
typedef enum NuOpenMode {
    kNuOpenUnknown,
    kNuOpenStreamingRO,
    kNuOpenRO,
    kNuOpenRW
} NuOpenMode;
#define Nu_IsStreaming(pArchive) ((pArchive)->openMode == kNuOpenStreamingRO)
#define Nu_IsReadOnly(pArchive)  ((pArchive)->openMode == kNuOpenStreamingRO ||\
                                  (pArchive)->openMode == kNuOpenRO)

#ifdef FOPEN_WANTS_B
# define kNuFileOpenReadOnly        "rb"
# define kNuFileOpenReadWrite       "r+b"
# define kNuFileOpenWriteTrunc      "wb"
# define kNuFileOpenReadWriteCreat  "w+b"
#else
# define kNuFileOpenReadOnly        "r"
# define kNuFileOpenReadWrite       "r+"
# define kNuFileOpenWriteTrunc      "w"
# define kNuFileOpenReadWriteCreat  "w+"
#endif


/*
 * Some NuFX and Binary II definitions.
 */
#define kNuMasterHeaderSize     48  /* size of fixed-length master header */
#define kNuRecordHeaderBaseSize 58  /* size of rec hdr up to variable stuff */
#define kNuThreadHeaderSize     16  /* size of fixed-length thread header */
#define kNuDefaultFilenameThreadSize    32  /* default size of filename thred */
#define kNuDefaultCommentSize   200 /* size of GSHK-mimic comments */
#define kNuBinary2BlockSize     128 /* size of bxy header and padding */
#define kNuSEAOffset            0x2ee5  /* fixed(??) offset to data in SEA */

#define kNuInitialChunkCRC      0x0000  /* start for CRC in LZW/1 chunk */
#define kNuInitialThreadCRC     0xffff  /* start for CRC in v3 thread header */

/* size of general-purpose compression buffer */
#define kNuGenCompBufSize       32768

#define kNuCharLF   0x0a
#define kNuCharCR   0x0d


/*
 * A list of records.  Generally we use one of these for read-only
 * archives, and two for read-write.
 *
 * The "loaded" flag is set when we've made some use of the record set.
 * Relying on "numRecords" won't always work; for example, if the "copy"
 * record set was initialized from "orig", and then had all of its records
 * deleted, you couldn't look at "numRecords" and decide whether it was
 * appropriate to use "orig" or not.
 */
typedef struct NuRecordSet {
    Boolean         loaded;
    uint32_t        numRecords;
    NuRecord*       nuRecordHead;
    NuRecord*       nuRecordTail;
} NuRecordSet;

/*
 * Archive state.
 */
struct NuArchive {
    uint32_t        structMagic;
    Boolean         busy;

    NuOpenMode      openMode;
    Boolean         newlyCreated;
    UNICHAR*        archivePathnameUNI;     /* pathname or "(stream)" */
    FILE*           archiveFp;
    NuArchiveType   archiveType;

    /* stuff before NuFX; both offsets are from 0, i.e. hdrOff includes junk */
    long            junkOffset;             /* skip past leading junk */
    long            headerOffset;           /* adjustment for BXY/SEA/BSE */

    UNICHAR*        tmpPathnameUNI;         /* temp file, for writes */
    FILE*           tmpFp;

    /* used during initial processing; helps avoid ftell() calls */
    long            currentOffset;

    /* setting this changes Extract into Test */
    Boolean         testMode;

    /* clumsy way of remembering name used for other fork in forked file */
    const UNICHAR*  lastFileCreatedUNI;
    /* clumsy way to avoid trying to create the same subdir several times */
    const UNICHAR*  lastDirCreatedUNI;

    /* master header from the archive */
    NuMasterHeader  masterHeader;           /* original */
    NuMasterHeader  newMasterHeader;        /* working copy during update */

    /* list of records from archive, plus some extra state */
    NuRecordIdx     recordIdxSeed;          /* where the NuRecordIdxs start */
    NuRecordIdx     nextRecordIdx;          /* next record gets this value */
    Boolean         haveToc;                /* set if we have full TOC */
    NuRecordSet     origRecordSet;          /* records from archive */
    NuRecordSet     copyRecordSet;          /* copy of orig, for write ops */
    NuRecordSet     newRecordSet;           /* newly-added records */

    /* state for compression functions */
    uint8_t*        compBuf;                /* large general-purpose buffer */
    void*           lzwCompressState;       /* state for LZW/1 and LZW/2 */
    void*           lzwExpandState;         /* state for LZW/1 and LZW/2 */

    /* options and attributes that the user can set */
    /* (these can be changed by a callback, so don't cache them internally) */
    void*           extraData;              /* application-defined pointer */

    NuValue         valAllowDuplicates;     /* allow dups when adding? */
    NuValue         valConvertExtractedEOL; /* convert EOL during extract? */
    NuValue         valDataCompression;     /* how to compress when adding? */
    NuValue         valDiscardWrapper;      /* remove BNY or SEA header? */
    NuValue         valEOL;                 /* EOL value to convert to */
    NuValue         valHandleExisting;      /* how to deal with existing files*/
    NuValue         valIgnoreCRC;           /* don't compute or test CRCs */
    NuValue         valMaskDataless;        /* alter Records w/o data threads */
    NuValue         valMimicSHK;            /* mimic some ShrinkIt quirks */
    NuValue         valModifyOrig;          /* modify original arc in place? */
    NuValue         valOnlyUpdateOlder;     /* modify original arc in place? */
    NuValue         valStripHighASCII;      /* during EOL conv, strip hi bit? */
    NuValue         valJunkSkipMax;         /* scan this far for header */
    NuValue         valIgnoreLZW2Len;       /* don't verify LZW/II len field */
    NuValue         valHandleBadMac;        /* handle "bad Mac" archives */

    /* callback functions */
    NuCallback      selectionFilterFunc;
    NuCallback      outputPathnameFunc;
    NuCallback      progressUpdaterFunc;
    NuCallback      errorHandlerFunc;
    NuCallback      messageHandlerFunc;
};

#define kNuArchiveStructMagic   0xc0edbabe

#define kNuDefaultRecordName    "UNKNOWN"   /* use ASCII charset */


/*
 * ===========================================================================
 *      ThreadMod definition
 * ===========================================================================
 */

/* operations we can perform on threads in a record */
typedef enum ThreadModKind {
    kNuThreadModUnknown = 0,

    kNuThreadModAdd,
    kNuThreadModUpdate,
    kNuThreadModDelete
} ThreadModKind;

/*
 * We attach a list of these to records we plan to modify.  Care is taken
 * to ensure that they don't conflict, e.g. you can't update a thread
 * right after you delete it, nor delete one you have modified.
 */
struct NuThreadMod {
    union {
        ThreadModKind       kind;

        struct {
            ThreadModKind   kind;
            Boolean         used;
        } generic;

        struct {
            ThreadModKind   kind;
            Boolean         used;
            Boolean         isPresized;
            NuThreadIdx     threadIdx;
            NuThreadID      threadID;
            NuThreadFormat  threadFormat;
            NuDataSource*   pDataSource;
        } add;

        struct {
            ThreadModKind   kind;
            Boolean         used;
            NuThreadIdx     threadIdx;
            NuDataSource*   pDataSource;
        } update;

        struct {
            ThreadModKind   kind;
            Boolean         used;
            NuThreadIdx     threadIdx;
            NuThreadID      threadID;   /* used for watching filename threads */
        } delete;
    } entry;

    struct NuThreadMod*     pNext;
};


/*
 * ===========================================================================
 *      NuFunnel/NuStraw definition
 * ===========================================================================
 */

#define kNuFunnelBufSize    16384

/*
 * File funnel definition.  This is used for writing output to files
 * (so we can do things like pipe compressed output through an LF->CR
 * converter) and archive files (so we can halt compression when the
 * output size exceeds the uncompressed original).  [ for various reasons,
 * I'm not using this on the archive anymore. ]
 *
 * Funnels are unidirectional.  You write data into them with a
 * function call; the top-level action (which is usually compressing or
 * expanding data) reads from the input and crams things into the pipe.
 * We could fully abstract the concept, and write the compression
 * functions so that they operate as a Funnel filter, but it's much
 * easier to write block-oriented compression than stream-oriented (and
 * more to the point, the ShrinkIt LZW functions are very much
 * block-oriented).
 */
typedef struct NuFunnel {
    /* data storage */
    uint8_t*        buffer;         /* kNuFunnelBufSize worth of storage */
    long            bufCount;       /* #of bytes in buffer */

    /* text conversion; if "auto", on first flush we convert to "on" or "off" */
    NuValue         convertEOL;     /* on/off/auto */
    NuValue         convertEOLTo;   /* EOL to switch to */
    NuValue         convertEOLFrom; /* EOL terminator we think we found */
    Boolean         checkStripHighASCII;    /* do we want to check for it? */
    Boolean         doStripHighASCII;   /* strip high ASCII during EOL conv */
    Boolean         lastCR;         /* was last char a CR? */

    Boolean         isFirstWrite;   /* cleared on first write */

#if 0
    uint32_t        inCount;        /* total #of bytes in the top */
    uint32_t        outCount;       /* total #of bytes out the bottom */

    uint32_t        outMax;         /* flag an err when outCount exceeds this */
    Boolean         outMaxExceeded; /* in fact, it's this flag */
#endif

    /* update this when stuff happens */
    NuProgressData* pProgress;

    /* data goeth out here */
    NuDataSink*     pDataSink;
} NuFunnel;


/*
 * File straw definition.  This is used for slurping up input data.
 *
 * Mostly this is an encapsulation of an input source and a progress
 * updater, useful for reading uncompressed data and feeding it to a
 * compressor.  It doesn't make sense to show a thermometer based on
 * compressed output, since we don't know how big the eventual result
 * will be, so we want to do it for the input.
 */
typedef struct NuStraw {
    /* update this when stuff happens */
    NuProgressData* pProgress;

    /* data cometh in here */
    NuDataSource*   pDataSource;

    /* progress update fields */
    uint32_t        lastProgress;
    uint32_t        lastDisplayed;
} NuStraw;

/*NuError Nu_CopyStreamToStream(FILE* outfp, FILE* infp, uint32_t count);*/


/*
 * ===========================================================================
 *      Data source and sink abstractions
 * ===========================================================================
 */

/*
 * DataSource is used when adding data to an archive.
 */

typedef enum NuDataSourceType {
    kNuDataSourceUnknown = 0,
    kNuDataSourceFromFile,
    kNuDataSourceFromFP,
    kNuDataSourceFromBuffer
} NuDataSourceType;

typedef struct NuDataSourceCommon {
    NuDataSourceType    sourceType;
    NuThreadFormat      threadFormat;       /* is it already compressed? */
    uint16_t            rawCrc;             /* crc for already-compressed data*/
    /*Boolean             doClose;            \* close on completion? */
    uint32_t            dataLen;            /* length of data (var for buf) */
    uint32_t            otherLen;           /* uncomp len or preset buf size */
    int                 refCount;           /* so we can copy structs */
} NuDataSourceCommon;

union NuDataSource {
    NuDataSourceType    sourceType;

    NuDataSourceCommon  common;

    struct {
        NuDataSourceCommon  common;
        UNICHAR*            pathnameUNI;
        Boolean             fromRsrcFork;

        /* temp storage; only valid when processing in library */
        FILE*               fp;
    } fromFile;

    struct {
        NuDataSourceCommon  common;
        FILE*               fp;
        long                offset;         /* starting offset */

        NuCallback          fcloseFunc;     /* how to fclose the file */
    } fromFP;

    struct {
        NuDataSourceCommon  common;
        const uint8_t*      buffer;         /* non-const if doClose=true */
        long                offset;         /* starting offset */

        long                curOffset;      /* current offset */
        long                curDataLen;     /* remaining data */

        NuCallback          freeFunc;       /* how to free data */
    } fromBuffer;
};


/*
 * DataSink is used when extracting data from an archive.
 */

typedef enum NuDataSinkType {
    kNuDataSinkUnknown = 0,
    kNuDataSinkToFile,
    kNuDataSinkToFP,
    kNuDataSinkToBuffer,
    kNuDataSinkToVoid
} NuDataSinkType;

typedef struct NuDataSinkCommon {
    NuDataSinkType      sinkType;
    Boolean             doExpand;       /* expand file? */
    NuValue             convertEOL;     /* convert EOL?  (req "expand") */
    uint32_t            outCount;
} NuDataSinkCommon;

union NuDataSink {
    NuDataSinkType      sinkType;

    NuDataSinkCommon    common;

    struct {
        NuDataSinkCommon    common;
        UNICHAR*            pathnameUNI;   /* file to open */
        UNICHAR             fssep;

        /* temp storage; must be NULL except when processing in library */
        FILE*               fp;
    } toFile;

    struct {
        NuDataSinkCommon    common;
        FILE*               fp;
    } toFP;

    struct {
        NuDataSinkCommon    common;
        uint8_t*            buffer;
        uint32_t            bufLen;     /* max amount of data "buffer" holds */
        NuError             stickyErr;
    } toBuffer;
};


/*
 * ===========================================================================
 *      Function prototypes
 * ===========================================================================
 */

/*
 * This is a little unpleasant.  This blob of stuff gets stuffed in as
 * the first arguments to Nu_ReportError, so we don't have to type them
 * in every time we use the function.  It would've been much easier to
 * use a gcc-style varargs macro, but not everybody uses gcc.
 */
#ifdef DEBUG_MSGS
 #ifdef HAS__FUNCTION__
  #define _FUNCTION_    __FUNCTION__
 #else
  #define _FUNCTION_    ""
 #endif

 #define NU_BLOB        pArchive, __FILE__, __LINE__, _FUNCTION_, false
 #define NU_BLOB_DEBUG  pArchive, __FILE__, __LINE__, _FUNCTION_, true
 #define NU_NILBLOB     NULL, __FILE__, __LINE__, _FUNCTION_, false
 #define DebugShowError(err)                                        \
        Nu_ReportError(pArchive, __FILE__, __LINE__, _FUNCTION_,    \
            true, err, "(DEBUG)");
#else
 #define NU_BLOB        pArchive, "", 0, "", false
 #define NU_BLOB_DEBUG  pArchive, "", 0, "", true
 #define NU_NILBLOB     NULL, "", 0, "", false
 #define DebugShowError(err)    ((void)0)
#endif

/*
 * The BailError macro serves two purposes.  First, it's a convenient
 * way to avoid typing, "if (err != kNuErrNone) goto bail;".  Second,
 * when the library is built with debugging enabled, it vitually gives
 * us a stack trace of exiting functions.  This makes it easier to debug
 * problems sent in as screen dumps via e-mail.
 */
#define BailError(err)  {                                           \
            if ((err) != kNuErrNone) {                              \
                /* [should this be debug-only, or all the time?] */ \
                DebugShowError(err);                                \
                goto bail;                                          \
            }                                                       \
        }
#define BailErrorQuiet(err) {                                       \
            if ((err) != kNuErrNone)                                \
                goto bail;                                          \
        }
#define BailNil(val)    {                                           \
            if ((val) == NULL) {                                    \
                err = kNuErrUnexpectedNil;                          \
                BailError(err);                                     \
            }                                                       \
        }
#define BailAlloc(val)  {                                           \
            if ((val) == NULL) {                                    \
                err = kNuErrMalloc;                                 \
                BailError(err);                                     \
            }                                                       \
        }


/*
 * Internal function prototypes and inline functions.
 */

/* Archive.c */
void Nu_MasterHeaderCopy(NuArchive* pArchive, NuMasterHeader* pDstHeader,
    const NuMasterHeader* pSrcHeader);
NuError Nu_GetMasterHeader(NuArchive* pArchive,
    const NuMasterHeader** ppMasterHeader);
NuRecordIdx Nu_GetNextRecordIdx(NuArchive* pArchive);
NuThreadIdx Nu_GetNextThreadIdx(NuArchive* pArchive);
NuError Nu_CopyWrapperToTemp(NuArchive* pArchive);
NuError Nu_UpdateWrapper(NuArchive* pArchive, FILE* fp);
NuError Nu_AdjustWrapperPadding(NuArchive* pArchive, FILE* fp);
NuError Nu_AllocCompressionBufferIFN(NuArchive* pArchive);
NuError Nu_StreamOpenRO(FILE* infp, NuArchive** ppArchive);
NuError Nu_OpenRO(const UNICHAR* archivePathnameUNI, NuArchive** ppArchive);
NuError Nu_OpenRW(const UNICHAR* archivePathnameUNI,
    const UNICHAR* tempPathnameUNI, uint32_t flags, NuArchive** ppArchive);
NuError Nu_WriteMasterHeader(NuArchive* pArchive, FILE* fp,
    NuMasterHeader* pMasterHeader);
NuError Nu_Close(NuArchive* pArchive);
NuError Nu_Abort(NuArchive* pArchive);
NuError Nu_RenameTempToArchive(NuArchive* pArchive);
NuError Nu_DeleteArchiveFile(NuArchive* pArchive);

/* ArchiveIO.c */
uint8_t Nu_ReadOneC(NuArchive* pArchive, FILE* fp, uint16_t* pCrc);
uint8_t Nu_ReadOne(NuArchive* pArchive, FILE* fp);
void Nu_WriteOneC(NuArchive* pArchive, FILE* fp, uint8_t val, uint16_t* pCrc);
void Nu_WriteOne(NuArchive* pArchive, FILE* fp, uint8_t val);
uint16_t Nu_ReadTwoC(NuArchive* pArchive, FILE* fp, uint16_t* pCrc);
uint16_t Nu_ReadTwo(NuArchive* pArchive, FILE* fp);
void Nu_WriteTwoC(NuArchive* pArchive, FILE* fp, uint16_t val, uint16_t* pCrc);
void Nu_WriteTwo(NuArchive* pArchive, FILE* fp, uint16_t val);
uint32_t Nu_ReadFourC(NuArchive* pArchive, FILE* fp, uint16_t* pCrc);
uint32_t Nu_ReadFour(NuArchive* pArchive, FILE* fp);
void Nu_WriteFourC(NuArchive* pArchive, FILE* fp, uint32_t val, uint16_t* pCrc);
void Nu_WriteFour(NuArchive* pArchive, FILE* fp, uint32_t val);
NuDateTime Nu_ReadDateTimeC(NuArchive* pArchive, FILE* fp, uint16_t* pCrc);
NuDateTime Nu_ReadDateTime(NuArchive* pArchive, FILE* fp, uint16_t* pCrc);
void Nu_WriteDateTimeC(NuArchive* pArchive, FILE* fp, NuDateTime dateTime,
    uint16_t* pCrc);
void Nu_WriteDateTime(NuArchive* pArchive, FILE* fp, NuDateTime dateTime);
void Nu_ReadBytesC(NuArchive* pArchive, FILE* fp, void* vbuffer, long count,
    uint16_t* pCrc);
void Nu_ReadBytes(NuArchive* pArchive, FILE* fp, void* vbuffer, long count);
void Nu_WriteBytesC(NuArchive* pArchive, FILE* fp, const void* vbuffer,
    long count, uint16_t* pCrc);
void Nu_WriteBytes(NuArchive* pArchive, FILE* fp, const void* vbuffer,
    long count);
NuError Nu_HeaderIOFailed(NuArchive* pArchive, FILE* fp);
NuError Nu_SeekArchive(NuArchive* pArchive, FILE* fp, long offset,
    int ptrname);
NuError Nu_RewindArchive(NuArchive* pArchive);

/* Bzip2.c */
NuError Nu_CompressBzip2(NuArchive* pArchive, NuStraw* pStraw, FILE* fp,
    uint32_t srcLen, uint32_t* pDstLen, uint16_t* pCrc);
NuError Nu_ExpandBzip2(NuArchive* pArchive, const NuRecord* pRecord,
    const NuThread* pThread, FILE* infp, NuFunnel* pFunnel, uint16_t* pCrc);

/* Charset.c */
size_t Nu_ConvertMORToUNI(const char* stringMOR, UNICHAR* bufUNI,
    size_t bufSize);
UNICHAR* Nu_CopyMORToUNI(const char* stringMOR);
size_t Nu_ConvertUNIToMOR(const UNICHAR* stringUNI, char* bufMOR,
    size_t bufSize);

/* Compress.c */
NuError Nu_CompressToArchive(NuArchive* pArchive, NuDataSource* pDataSource,
    NuThreadID threadID, NuThreadFormat sourceFormat,
    NuThreadFormat targetFormat, NuProgressData* progressData, FILE* dstFp,
    NuThread* pThread);
NuError Nu_CopyPresizedToArchive(NuArchive* pArchive,
    NuDataSource* pDataSource, NuThreadID threadID, FILE* dstFp,
    NuThread* pThread, char** ppSavedCopy);

/* Crc16.c */
extern const uint16_t gNuCrc16Table[256];
uint16_t Nu_CalcCRC16(uint16_t seed, const uint8_t* ptr, int count);
/*
 * Update the CRC-16.
 *
 * _val (uint8_t) is the byte to add to the CRC.  It's evaluated once.
 * _crc (uint16_t) is the previous CRC.  It's evaluated twice.
 * Returns the updated CRC as a uint16_t.
 */
#define Nu_UpdateCRC16(_val, _crc) \
    (gNuCrc16Table[(((_crc) >> 8) & 0xff) ^ (_val)] ^ ((_crc) << 8))

/* Debug.c */
#if defined(DEBUG_MSGS) || !defined(NDEBUG)
void Nu_DebugDumpAll(NuArchive* pArchive);
void Nu_DebugDumpThread(const NuThread* pThread);
#endif

/* Deferred.c */
NuError Nu_ThreadModAdd_New(NuArchive* pArchive, NuThreadID threadID,
    NuThreadFormat threadFormat, NuDataSource* pDataSource,
    NuThreadMod** ppThreadMod);
NuError Nu_ThreadModUpdate_New(NuArchive* pArchive, NuThreadIdx threadIdx,
    NuDataSource* pDataSource, NuThreadMod** ppThreadMod);
NuError Nu_ThreadModDelete_New(NuArchive* pArchive, NuThreadIdx threadIdx,
    NuThreadID threadID, NuThreadMod** ppThreadMod);
void Nu_ThreadModFree(NuArchive* pArchive, NuThreadMod* pThreadMod);
NuError Nu_ThreadModAdd_FindByThreadID(const NuRecord* pRecord,
    NuThreadID threadID, NuThreadMod** ppThreadMod);
void Nu_FreeThreadMods(NuArchive* pArchive, NuRecord* pRecord);
NuThreadMod* Nu_ThreadMod_FindByThreadIdx(const NuRecord* pRecord,
    NuThreadIdx threadIdx);
NuError Nu_Flush(NuArchive* pArchive, uint32_t* pStatusFlags);

/* Deflate.c */
NuError Nu_CompressDeflate(NuArchive* pArchive, NuStraw* pStraw, FILE* fp,
    uint32_t srcLen, uint32_t* pDstLen, uint16_t* pCrc);
NuError Nu_ExpandDeflate(NuArchive* pArchive, const NuRecord* pRecord,
    const NuThread* pThread, FILE* infp, NuFunnel* pFunnel, uint16_t* pCrc);

/* Expand.c */
NuError Nu_ExpandStream(NuArchive* pArchive, const NuRecord* pRecord,
    const NuThread* pThread, FILE* infp, NuFunnel* pFunnel);

/* FileIO.c */
void Nu_SetCurrentDateTime(NuDateTime* pDateTime);
Boolean Nu_IsOlder(const NuDateTime* pWhen1, const NuDateTime* pWhen2);
NuError Nu_OpenOutputFile(NuArchive* pArchive, const NuRecord* pRecord,
    const NuThread* pThread, const UNICHAR* newPathnameUNI, UNICHAR newFssep,
    FILE** pFp);
NuError Nu_CloseOutputFile(NuArchive* pArchive, const NuRecord* pRecord,
    FILE* fp, const UNICHAR* pathnameUNI);
NuError Nu_OpenInputFile(NuArchive* pArchive, const UNICHAR* pathnameUNI,
    Boolean openRsrc, FILE** pFp);
NuError Nu_DeleteFile(const UNICHAR* pathnameUNI);
NuError Nu_RenameFile(const UNICHAR* fromPathUNI, const UNICHAR* toPathUNI);
NuError Nu_FTell(FILE* fp, long* pOffset);
NuError Nu_FSeek(FILE* fp, long offset, int ptrname);
NuError Nu_FRead(FILE* fp, void* buf, size_t nbyte);
NuError Nu_FWrite(FILE* fp, const void* buf, size_t nbyte);
NuError Nu_CopyFileSection(NuArchive* pArchive, FILE* dstFp, FILE* srcFp,
    long length);
NuError Nu_GetFileLength(NuArchive* pArchive, FILE* fp, long* pLength);
NuError Nu_TruncateOpenFile(FILE* fp, long length);

/* Funnel.c */
NuError Nu_ProgressDataInit_Compress(NuArchive* pArchive,
    NuProgressData* pProgressData, const NuRecord* pRecord,
    const UNICHAR* origPathnameUNI, const UNICHAR* pathnameUNI);
NuError Nu_ProgressDataInit_Expand(NuArchive* pArchive,
    NuProgressData* pProgressData, const NuRecord* pRecord,
    const UNICHAR* newPathnameUNI, UNICHAR newFssep,
    const UNICHAR* origPathnameUNI, NuValue convertEOL);
NuError Nu_SendInitialProgress(NuArchive* pArchive, NuProgressData* pProgress);

NuError Nu_FunnelNew(NuArchive* pArchive, NuDataSink* pDataSink,
    NuValue convertEOL, NuValue convertEOLTo, NuProgressData* pProgress,
    NuFunnel** ppFunnel);
NuError Nu_FunnelFree(NuArchive* pArchive, NuFunnel* pFunnel);
/*void Nu_FunnelSetMaxOutput(NuFunnel* pFunnel, uint32_t maxBytes);*/
NuError Nu_FunnelWrite(NuArchive* pArchive, NuFunnel* pFunnel,
    const uint8_t* buffer, uint32_t count);
NuError Nu_FunnelFlush(NuArchive* pArchive, NuFunnel* pFunnel);
NuError Nu_ProgressDataCompressPrep(NuArchive* pArchive, NuStraw* pStraw,
    NuThreadFormat threadFormat, uint32_t sourceLen);
NuError Nu_ProgressDataExpandPrep(NuArchive* pArchive, NuFunnel* pFunnel,
    const NuThread* pThread);
NuError Nu_FunnelSetProgressState(NuFunnel* pFunnel, NuProgressState state);
NuError Nu_FunnelSendProgressUpdate(NuArchive* pArchive, NuFunnel* pFunnel);
Boolean Nu_FunnelGetDoExpand(NuFunnel* pFunnel);

NuError Nu_StrawNew(NuArchive* pArchive, NuDataSource* pDataSource,
    NuProgressData* pProgress, NuStraw** ppStraw);
NuError Nu_StrawFree(NuArchive* pArchive, NuStraw* pStraw);
NuError Nu_StrawSetProgressState(NuStraw* pStraw, NuProgressState state);
NuError Nu_StrawSendProgressUpdate(NuArchive* pArchive, NuStraw* pStraw);
NuError Nu_StrawRead(NuArchive* pArchive, NuStraw* pStraw, uint8_t* buffer,
    long len);
NuError Nu_StrawRewind(NuArchive* pArchive, NuStraw* pStraw);

/* Lzc.c */
NuError Nu_CompressLZC12(NuArchive* pArchive, NuStraw* pStraw, FILE* fp,
    uint32_t srcLen, uint32_t* pDstLen, uint16_t* pCrc);
NuError Nu_CompressLZC16(NuArchive* pArchive, NuStraw* pStraw, FILE* fp,
    uint32_t srcLen, uint32_t* pDstLen, uint16_t* pCrc);
NuError Nu_ExpandLZC(NuArchive* pArchive, const NuRecord* pRecord,
    const NuThread* pThread, FILE* infp, NuFunnel* pFunnel, uint16_t* pCrc);

/* Lzw.c */
NuError Nu_CompressLZW1(NuArchive* pArchive, NuStraw* pStraw, FILE* fp,
    uint32_t srcLen, uint32_t* pDstLen, uint16_t* pCrc);
NuError Nu_CompressLZW2(NuArchive* pArchive, NuStraw* pStraw, FILE* fp,
    uint32_t srcLen, uint32_t* pDstLen, uint16_t* pCrc);
NuError Nu_ExpandLZW(NuArchive* pArchive, const NuRecord* pRecord,
    const NuThread* pThread, FILE* infp, NuFunnel* pFunnel,
    uint16_t* pThreadCrc);

/* MiscUtils.c */
/*extern const char* kNufxLibName;*/
extern NuCallback gNuGlobalErrorMessageHandler;
const char* Nu_StrError(NuError err);
void Nu_ReportError(NuArchive* pArchive, const char* file, int line,
    const char* function, Boolean isDebug, NuError err,
    const UNICHAR* format, ...)
    #if defined(__GNUC__)
        __attribute__ ((format(printf, 7, 8)))
    #endif
    ;
#ifdef USE_DMALLOC  /* want file and line numbers for calls */
# define Nu_Malloc(archive, size) malloc(size)
# define Nu_Calloc(archive, size) calloc(1, size)
# define Nu_Realloc(archive, ptr, size) realloc(ptr, size)
# define Nu_Free(archive, ptr) (ptr != NULL ? free(ptr) : (void)0)
#else
void* Nu_Malloc(NuArchive* pArchive, size_t size);
void* Nu_Calloc(NuArchive* pArchive, size_t size);
void* Nu_Realloc(NuArchive* pArchive, void* ptr, size_t size);
void Nu_Free(NuArchive* pArchive, void* ptr);
#endif
NuResult Nu_InternalFreeCallback(NuArchive* pArchive, void* args);

/* Record.c */
void Nu_RecordAddThreadMod(NuRecord* pRecord, NuThreadMod* pThreadMod);
Boolean Nu_RecordIsEmpty(NuArchive* pArchive, const NuRecord* pRecord);
Boolean Nu_RecordSet_GetLoaded(const NuRecordSet* pRecordSet);
uint32_t Nu_RecordSet_GetNumRecords(const NuRecordSet* pRecordSet);
void Nu_RecordSet_SetNumRecords(NuRecordSet* pRecordSet, uint32_t val);
void Nu_RecordSet_IncNumRecords(NuRecordSet* pRecordSet);
NuRecord* Nu_RecordSet_GetListHead(const NuRecordSet* pRecordSet);
NuRecord** Nu_RecordSet_GetListHeadPtr(NuRecordSet* pRecordSet);
NuRecord* Nu_RecordSet_GetListTail(const NuRecordSet* pRecordSet);
Boolean Nu_RecordSet_IsEmpty(const NuRecordSet* pRecordSet);
NuError Nu_RecordSet_FreeAllRecords(NuArchive* pArchive,
    NuRecordSet* pRecordSet);
NuError Nu_RecordSet_DeleteRecordPtr(NuArchive* pArchive,
    NuRecordSet* pRecordSet, NuRecord** ppRecord);
NuError Nu_RecordSet_DeleteRecord(NuArchive* pArchive, NuRecordSet* pRecordSet,
    NuRecord* pRecord);
NuError Nu_RecordSet_Clone(NuArchive* pArchive, NuRecordSet* pDstSet,
    const NuRecordSet* pSrcSet);
NuError Nu_RecordSet_MoveAllRecords(NuArchive* pArchive, NuRecordSet* pDstSet,
    NuRecordSet* pSrcSet);
NuError Nu_RecordSet_FindByIdx(const NuRecordSet* pRecordSet, NuRecordIdx rec,
    NuRecord** ppRecord);
NuError Nu_RecordSet_FindByThreadIdx(NuRecordSet* pRecordSet,
    NuThreadIdx threadIdx, NuRecord** ppRecord, NuThread** ppThread);
NuError Nu_RecordSet_ReplaceRecord(NuArchive* pArchive, NuRecordSet* pBadSet,
    NuRecord* pBadRecord, NuRecordSet* pGoodSet, NuRecord** ppGoodRecord);
Boolean Nu_ShouldIgnoreBadCRC(NuArchive* pArchive, const NuRecord* pRecord,
    NuError err);
NuError Nu_WriteRecordHeader(NuArchive* pArchive, NuRecord* pRecord, FILE* fp);
NuError Nu_GetTOCIfNeeded(NuArchive* pArchive);
NuError Nu_StreamContents(NuArchive* pArchive, NuCallback contentFunc);
NuError Nu_StreamExtract(NuArchive* pArchive);
NuError Nu_StreamTest(NuArchive* pArchive);
NuError Nu_Contents(NuArchive* pArchive, NuCallback contentFunc);
NuError Nu_Extract(NuArchive* pArchive);
NuError Nu_ExtractRecord(NuArchive* pArchive, NuRecordIdx recIdx);
NuError Nu_Test(NuArchive* pArchive);
NuError Nu_TestRecord(NuArchive* pArchive, NuRecordIdx recIdx);
NuError Nu_GetRecord(NuArchive* pArchive, NuRecordIdx recordIdx,
    const NuRecord** ppRecord);
NuError Nu_GetRecordIdxByName(NuArchive* pArchive, const char* nameMOR,
    NuRecordIdx* pRecordIdx);
NuError Nu_GetRecordIdxByPosition(NuArchive* pArchive, uint32_t position,
    NuRecordIdx* pRecordIdx);
NuError Nu_FindRecordForWriteByIdx(NuArchive* pArchive, NuRecordIdx recIdx,
    NuRecord** ppFoundRecord);
NuError Nu_AddFile(NuArchive* pArchive, const UNICHAR* pathnameUNI,
    const NuFileDetails* pFileDetails, Boolean fromRsrcFork,
    NuRecordIdx* pRecordIdx);
NuError Nu_AddRecord(NuArchive* pArchive, const NuFileDetails* pFileDetails,
    NuRecordIdx* pRecordIdx, NuRecord** ppRecord);
NuError Nu_Rename(NuArchive* pArchive, NuRecordIdx recIdx,
    const char* pathnameMOR, char fssepMOR);
NuError Nu_SetRecordAttr(NuArchive* pArchive, NuRecordIdx recordIdx,
    const NuRecordAttr* pRecordAttr);
NuError Nu_Delete(NuArchive* pArchive);
NuError Nu_DeleteRecord(NuArchive* pArchive, NuRecordIdx rec);

/* SourceSink.c */
NuError Nu_DataSourceFile_New(NuThreadFormat threadFormat,
    uint32_t otherLen, const UNICHAR* pathnameUNI, Boolean isFromRsrcFork,
    NuDataSource** ppDataSource);
NuError Nu_DataSourceFP_New(NuThreadFormat threadFormat,
    uint32_t otherLen, FILE* fp, long offset, long length,
    NuCallback fcloseFunc, NuDataSource** ppDataSource);
NuError Nu_DataSourceBuffer_New(NuThreadFormat threadFormat,
    uint32_t otherLen, const uint8_t* buffer, long offset, long length,
    NuCallback freeFunc, NuDataSource** ppDataSource);
NuDataSource* Nu_DataSourceCopy(NuDataSource* pDataSource);
NuError Nu_DataSourceFree(NuDataSource* pDataSource);
NuDataSourceType Nu_DataSourceGetType(const NuDataSource* pDataSource);
NuThreadFormat Nu_DataSourceGetThreadFormat(const NuDataSource* pDataSource);
uint32_t Nu_DataSourceGetDataLen(const NuDataSource* pDataSource);
uint32_t Nu_DataSourceGetOtherLen(const NuDataSource* pDataSource);
void Nu_DataSourceSetOtherLen(NuDataSource* pDataSource, long otherLen);
uint16_t Nu_DataSourceGetRawCrc(const NuDataSource* pDataSource);
void Nu_DataSourceSetRawCrc(NuDataSource* pDataSource, uint16_t crc);
NuError Nu_DataSourcePrepareInput(NuArchive* pArchive,
    NuDataSource* pDataSource);
void Nu_DataSourceUnPrepareInput(NuArchive* pArchive,
    NuDataSource* pDataSource);
const char* Nu_DataSourceFile_GetPathname(NuDataSource* pDataSource);
NuError Nu_DataSourceGetBlock(NuDataSource* pDataSource, uint8_t* buf,
    uint32_t len);
NuError Nu_DataSourceRewind(NuDataSource* pDataSource);
NuError Nu_DataSinkFile_New(Boolean doExpand, NuValue convertEOL,
    const UNICHAR* pathnameUNI, UNICHAR fssep, NuDataSink** ppDataSink);
NuError Nu_DataSinkFP_New(Boolean doExpand, NuValue convertEOL, FILE* fp,
    NuDataSink** ppDataSink);
NuError Nu_DataSinkBuffer_New(Boolean doExpand, NuValue convertEOL,
    uint8_t* buffer, uint32_t bufLen, NuDataSink** ppDataSink);
NuError Nu_DataSinkVoid_New(Boolean doExpand, NuValue convertEOL,
    NuDataSink** ppDataSink);
NuError Nu_DataSinkFree(NuDataSink* pDataSink);
NuDataSinkType Nu_DataSinkGetType(const NuDataSink* pDataSink);
Boolean Nu_DataSinkGetDoExpand(const NuDataSink* pDataSink);
NuValue Nu_DataSinkGetConvertEOL(const NuDataSink* pDataSink);
uint32_t Nu_DataSinkGetOutCount(const NuDataSink* pDataSink);
const char* Nu_DataSinkFile_GetPathname(const NuDataSink* pDataSink);
UNICHAR Nu_DataSinkFile_GetFssep(const NuDataSink* pDataSink);
FILE* Nu_DataSinkFile_GetFP(const NuDataSink* pDataSink);
void Nu_DataSinkFile_SetFP(NuDataSink* pDataSink, FILE* fp);
void Nu_DataSinkFile_Close(NuDataSink* pDataSink);
NuError Nu_DataSinkPutBlock(NuDataSink* pDataSink, const uint8_t* buf,
    uint32_t len);
NuError Nu_DataSinkGetError(NuDataSink* pDataSink);

/* Squeeze.c */
NuError Nu_CompressHuffmanSQ(NuArchive* pArchive, NuStraw* pStraw, FILE* fp,
    uint32_t srcLen, uint32_t* pDstLen, uint16_t* pCrc);
NuError Nu_ExpandHuffmanSQ(NuArchive* pArchive, const NuRecord* pRecord,
    const NuThread* pThread, FILE* infp, NuFunnel* pFunnel, uint16_t* pCrc);

/* Thread.c */
NuThread* Nu_GetThread(const NuRecord* pRecord, int idx);
void Nu_StripHiIfAllSet(char* str);
Boolean Nu_IsPresizedThreadID(NuThreadID threadID);
Boolean Nu_IsCompressibleThreadID(NuThreadID threadID);
Boolean Nu_ThreadHasCRC(uint16_t recordVersion, NuThreadID threadID);
NuError Nu_FindThreadByIdx(const NuRecord* pRecord, NuThreadIdx thread,
    NuThread** ppThread);
NuError Nu_FindThreadByID(const NuRecord* pRecord, NuThreadID threadID,
    NuThread** ppThread);
void Nu_CopyThreadContents(NuThread* pDstThread, const NuThread* pSrcThread);
NuError Nu_ReadThreadHeaders(NuArchive* pArchive, NuRecord* pRecord,
    uint16_t* pCrc);
NuError Nu_WriteThreadHeaders(NuArchive* pArchive, NuRecord* pRecord, FILE* fp,
    uint16_t* pCrc);
NuError Nu_ComputeThreadData(NuArchive* pArchive, NuRecord* pRecord);
NuError Nu_ScanThreads(NuArchive* pArchive, NuRecord* pRecord,long numThreads);
NuError Nu_ExtractThreadBulk(NuArchive* pArchive, const NuRecord* pRecord,
    const NuThread* pThread);
NuError Nu_SkipThread(NuArchive* pArchive, const NuRecord* pRecord,
    const NuThread* pThread);
NuError Nu_ExtractThread(NuArchive* pArchive, NuThreadIdx threadIdx,
    NuDataSink* pDataSink);
NuError Nu_OkayToAddThread(NuArchive* pArchive, const NuRecord* pRecord,
    NuThreadID threadID);
NuError Nu_AddThread(NuArchive* pArchive, NuRecordIdx rec, NuThreadID threadID,
    NuDataSource* pDataSource, NuThreadIdx* pThreadIdx);
NuError Nu_UpdatePresizedThread(NuArchive* pArchive, NuThreadIdx threadIdx,
    NuDataSource* pDataSource, int32_t* pMaxLen);
NuError Nu_DeleteThread(NuArchive* pArchive, NuThreadIdx threadIdx);

/* Value.c */
NuError Nu_GetValue(NuArchive* pArchive, NuValueID ident, NuValue* pValue);
NuError Nu_SetValue(NuArchive* pArchive, NuValueID ident, NuValue value);
NuError Nu_GetAttr(NuArchive* pArchive, NuAttrID ident, NuAttr* pAttr);
NuThreadFormat Nu_ConvertCompressValToFormat(NuArchive* pArchive,
    NuValue compValue);

/* Version.c */
NuError Nu_GetVersion(int32_t* pMajorVersion, int32_t* pMinorVersion,
    int32_t* pBugVersion, const char** ppBuildDate, const char** ppBuildFlags);

#endif /*NUFXLIB_NUFXLIBPRIV_H*/
