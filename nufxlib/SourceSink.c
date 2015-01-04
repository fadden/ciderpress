/*
 * NuFX archive manipulation library
 * Copyright (C) 2000-2007 by Andy McFadden, All Rights Reserved.
 * This is free software; you can redistribute it and/or modify it under the
 * terms of the BSD License, see the file COPYING-LIB.
 *
 * Implementation of DataSource and DataSink objects.
 */
#include "NufxLibPriv.h"



/*
 * ===========================================================================
 *      NuDataSource
 * ===========================================================================
 */

/*
 * Allocate a new DataSource structure.
 */
static NuError Nu_DataSourceNew(NuDataSource** ppDataSource)
{
    Assert(ppDataSource != NULL);

    *ppDataSource = Nu_Malloc(NULL, sizeof(**ppDataSource));
    if (*ppDataSource == NULL)
        return kNuErrMalloc;

    (*ppDataSource)->sourceType = kNuDataSourceUnknown;

    return kNuErrNone;
}


/*
 * Make a copy of a DataSource.  Actually just increments a reference count.
 *
 * What we *really* want to be doing is copying the structure (since we
 * can't guarantee copy-on-write semantics for the fields without adding
 * more stuff) and refcounting the underlying resource, so that "auto-free"
 * semantics work out right.
 *
 * We're okay for now, since for the most part we only do work on one
 * copy of each.  (I wish I could remember why this copying thing was
 * needed in the first place.)  Buffer sources are a little scary since
 * they include a "curOffset" value.
 *
 * Returns NULL on error.
 */
NuDataSource* Nu_DataSourceCopy(NuDataSource* pDataSource)
{
    Assert(pDataSource->common.refCount >= 1);
    pDataSource->common.refCount++;
    return pDataSource;

#if 0   /* we used to copy them -- very bad idea */
    NuDataSource* pNewDataSource;

    Assert(pDataSource != NULL);

    if (Nu_DataSourceNew(&pNewDataSource) != kNuErrNone)
        return NULL;
    Assert(pNewDataSource != NULL);

    /* this gets most of it */
    memcpy(pNewDataSource, pDataSource, sizeof(*pNewDataSource));

    /* copy anything we're sure to free up */
    if (pDataSource->sourceType == kNuDataSourceFromFile) {
        Assert(pDataSource->fromFile.fp == NULL);        /* does this matter? */
        pNewDataSource->fromFile.pathname =
                                    strdup(pDataSource->fromFile.pathname);
    }

    /* don't let the original free up the resources */
    if (pDataSource->common.doClose) {
        DBUG(("--- clearing doClose on source-copy of data source\n"));
        pDataSource->common.doClose = false;
    }

    return pNewDataSource;
#endif
}


/*
 * Free a data source structure, and any type-specific elements.
 */
NuError Nu_DataSourceFree(NuDataSource* pDataSource)
{
    if (pDataSource == NULL)
        return kNuErrNone;

    Assert(pDataSource->common.refCount > 0);
    if (pDataSource->common.refCount > 1) {
        pDataSource->common.refCount--;
        return kNuErrNone;
    }

    switch (pDataSource->sourceType) {
    case kNuDataSourceFromFile:
        Nu_Free(NULL, pDataSource->fromFile.pathnameUNI);
        if (pDataSource->fromFile.fp != NULL) {
            fclose(pDataSource->fromFile.fp);
            pDataSource->fromFile.fp = NULL;
        }
        break;
    case kNuDataSourceFromFP:
        if (pDataSource->fromFP.fcloseFunc != NULL &&
            pDataSource->fromFP.fp != NULL)
        {
            (*pDataSource->fromFP.fcloseFunc)(NULL, pDataSource->fromFP.fp);
            pDataSource->fromFP.fp = NULL;
        }
        break;
    case kNuDataSourceFromBuffer:
        if (pDataSource->fromBuffer.freeFunc != NULL) {
            (*pDataSource->fromBuffer.freeFunc)(NULL,
                                        (void*)pDataSource->fromBuffer.buffer);
            pDataSource->fromBuffer.buffer = NULL;
        }
        break;
    case kNuDataSourceUnknown:
        break;
    default:
        Assert(0);
        return kNuErrInternal;
    }

    Nu_Free(NULL, pDataSource);
    return kNuErrNone;
}


/*
 * Create a data source for an unopened file.
 */
NuError Nu_DataSourceFile_New(NuThreadFormat threadFormat, uint32_t otherLen,
    const UNICHAR* pathnameUNI, Boolean isFromRsrcFork,
    NuDataSource** ppDataSource)
{
    NuError err;

    if (pathnameUNI == NULL ||
        !(isFromRsrcFork == true || isFromRsrcFork == false) ||
        ppDataSource == NULL)
    {
        return kNuErrInvalidArg;
    }

    err = Nu_DataSourceNew(ppDataSource);
    BailErrorQuiet(err);

    (*ppDataSource)->common.sourceType = kNuDataSourceFromFile;
    (*ppDataSource)->common.threadFormat = threadFormat;
    (*ppDataSource)->common.rawCrc = 0;
    (*ppDataSource)->common.dataLen = 0;    /* to be filled in later */
    (*ppDataSource)->common.otherLen = otherLen;
    (*ppDataSource)->common.refCount = 1;

    (*ppDataSource)->fromFile.pathnameUNI = strdup(pathnameUNI);
    (*ppDataSource)->fromFile.fromRsrcFork = isFromRsrcFork;
    (*ppDataSource)->fromFile.fp = NULL;     /* to be filled in later */

bail:
    return err;
}


/*
 * Create a data source for an open file at a specific offset.  The FILE*
 * must be seekable.
 */
NuError Nu_DataSourceFP_New(NuThreadFormat threadFormat, uint32_t otherLen,
    FILE* fp, long offset, long length, NuCallback fcloseFunc,
    NuDataSource** ppDataSource)
{
    NuError err;

    if (fp == NULL || offset < 0 || length < 0 ||
        ppDataSource == NULL)
    {
        return kNuErrInvalidArg;
    }

    if (otherLen && otherLen < (uint32_t)length) {
        DBUG(("--- rejecting FP len=%ld other=%ld\n", length, otherLen));
        err = kNuErrPreSizeOverflow;
        goto bail;
    }

    err = Nu_DataSourceNew(ppDataSource);
    BailErrorQuiet(err);

    (*ppDataSource)->common.sourceType = kNuDataSourceFromFP;
    (*ppDataSource)->common.threadFormat = threadFormat;
    (*ppDataSource)->common.rawCrc = 0;
    (*ppDataSource)->common.dataLen = length;
    (*ppDataSource)->common.otherLen = otherLen;
    (*ppDataSource)->common.refCount = 1;

    (*ppDataSource)->fromFP.fp = fp;
    (*ppDataSource)->fromFP.offset = offset;
    (*ppDataSource)->fromFP.fcloseFunc = fcloseFunc;

bail:
    return err;
}


/*
 * Create a data source for a buffer.
 *
 * We allow "buffer" to be NULL so long as "offset" and "length" are also
 * NULL.  This is useful for creating empty pre-sized buffers, such as
 * blank comment fields.
 */
NuError Nu_DataSourceBuffer_New(NuThreadFormat threadFormat, uint32_t otherLen,
    const uint8_t* buffer, long offset, long length, NuCallback freeFunc,
    NuDataSource** ppDataSource)
{
    NuError err;

    if (offset < 0 || length < 0 || ppDataSource == NULL)
        return kNuErrInvalidArg;
    if (buffer == NULL && (offset != 0 || length != 0))
        return kNuErrInvalidArg;

    if (buffer == NULL) {
        DBUG(("+++ zeroing freeFunc for empty-buffer DataSource\n"));
        freeFunc = NULL;
    }

    if (otherLen && otherLen < (uint32_t)length) {
        DBUG(("--- rejecting buffer len=%ld other=%ld\n", length, otherLen));
        err = kNuErrPreSizeOverflow;
        goto bail;
    }

    err = Nu_DataSourceNew(ppDataSource);
    BailErrorQuiet(err);

    (*ppDataSource)->common.sourceType = kNuDataSourceFromBuffer;
    (*ppDataSource)->common.threadFormat = threadFormat;
    (*ppDataSource)->common.rawCrc = 0;
    (*ppDataSource)->common.dataLen = length;
    (*ppDataSource)->common.otherLen = otherLen;
    (*ppDataSource)->common.refCount = 1;

    (*ppDataSource)->fromBuffer.buffer = buffer;
    (*ppDataSource)->fromBuffer.offset = offset;
    (*ppDataSource)->fromBuffer.curOffset = offset;
    (*ppDataSource)->fromBuffer.curDataLen = length;
    (*ppDataSource)->fromBuffer.freeFunc = freeFunc;

bail:
    return err;
}


/*
 * Get the type of a NuDataSource.
 */
NuDataSourceType Nu_DataSourceGetType(const NuDataSource* pDataSource)
{
    Assert(pDataSource != NULL);
    return pDataSource->sourceType;
}

/*
 * Get the threadFormat for a data source.
 */
NuThreadFormat Nu_DataSourceGetThreadFormat(const NuDataSource* pDataSource)
{
    Assert(pDataSource != NULL);
    return pDataSource->common.threadFormat;
}

/*
 * Get "dataLen" from a dataSource.
 */
uint32_t Nu_DataSourceGetDataLen(const NuDataSource* pDataSource)
{
    Assert(pDataSource != NULL);

    if (pDataSource->sourceType == kNuDataSourceFromFile) {
        /* dataLen can only be valid if file has been opened */
        Assert(pDataSource->fromFile.fp != NULL);
    }

    return pDataSource->common.dataLen;
}

/*
 * Get "otherLen" from a dataSource.
 */
uint32_t Nu_DataSourceGetOtherLen(const NuDataSource* pDataSource)
{
    Assert(pDataSource != NULL);
    return pDataSource->common.otherLen;
}

/*
 * Change the "otherLen" value.
 */
void Nu_DataSourceSetOtherLen(NuDataSource* pDataSource, long otherLen)
{
    Assert(pDataSource != NULL && otherLen > 0);
    pDataSource->common.otherLen = otherLen;
}


/*
 * Get the "raw CRC" value.
 */
uint16_t Nu_DataSourceGetRawCrc(const NuDataSource* pDataSource)
{
    Assert(pDataSource != NULL);
    return pDataSource->common.rawCrc;
}

/*
 * Set the "raw CRC" value.  You would want to do this if the input was
 * already-compressed data, and you wanted to propagate the thread CRC.
 */
void Nu_DataSourceSetRawCrc(NuDataSource* pDataSource, uint16_t crc)
{
    Assert(pDataSource != NULL);
    pDataSource->common.rawCrc = crc;
}


/*
 * Prepare a data source for action.
 */
NuError Nu_DataSourcePrepareInput(NuArchive* pArchive,
    NuDataSource* pDataSource)
{
    NuError err = kNuErrNone;
    FILE* fileFp = NULL;

    /*
     * Doesn't apply to buffer sources.
     */
    if (Nu_DataSourceGetType(pDataSource) == kNuDataSourceFromBuffer)
        goto bail;

    /*
     * FP sources can be used several times, so we need to seek them
     * to the correct offset before we begin.
     */
    if (Nu_DataSourceGetType(pDataSource) == kNuDataSourceFromFP) {
        err = Nu_FSeek(pDataSource->fromFP.fp, pDataSource->fromFP.offset,
                SEEK_SET);
        goto bail;      /* return this err */
    }

    /*
     * We're adding from a file on disk.  Open it.
     */
    err = Nu_OpenInputFile(pArchive,
            pDataSource->fromFile.pathnameUNI,
            pDataSource->fromFile.fromRsrcFork, &fileFp);
    BailError(err);

    Assert(fileFp != NULL);
    pDataSource->fromFile.fp = fileFp;
    err = Nu_GetFileLength(pArchive, fileFp,
            (long*)&pDataSource->common.dataLen);
    BailError(err);

    if (pDataSource->common.otherLen &&
        pDataSource->common.otherLen < pDataSource->common.dataLen)
    {
        DBUG(("--- Uh oh, looks like file len is too small for presized\n"));
    }

bail:
    return err;
}


/*
 * Un-prepare a data source.  This really only affects "file" sources, and
 * is only here so we don't end up with 200+ FILE* structures hanging around.
 * If we don't do this, the first resource we're likely to run out of is
 * file descriptors.
 *
 * It's not necessary to do this in all error cases -- the DataSource "Free"
 * call will take care of this eventually -- but for normal operation on
 * a large number of files, it's vital.
 */
void Nu_DataSourceUnPrepareInput(NuArchive* pArchive, NuDataSource* pDataSource)
{
    if (Nu_DataSourceGetType(pDataSource) != kNuDataSourceFromFile)
        return;

    if (pDataSource->fromFile.fp != NULL) {
        fclose(pDataSource->fromFile.fp);
        pDataSource->fromFile.fp = NULL;
        pDataSource->common.dataLen = 0;
    }
}


/*
 * Get the pathname from a "from-file" dataSource.  Returned string is UTF-8.
 */
const char* Nu_DataSourceFile_GetPathname(NuDataSource* pDataSource)
{
    Assert(pDataSource != NULL);
    Assert(pDataSource->sourceType == kNuDataSourceFromFile);
    Assert(pDataSource->fromFile.pathnameUNI != NULL);

    return pDataSource->fromFile.pathnameUNI;
}


/*
 * Read a block of data from a dataSource.
 */
NuError Nu_DataSourceGetBlock(NuDataSource* pDataSource, uint8_t* buf,
    uint32_t len)
{
    NuError err;

    Assert(pDataSource != NULL);
    Assert(buf != NULL);
    Assert(len > 0);

    switch (pDataSource->sourceType) {
    case kNuDataSourceFromFile:
        Assert(pDataSource->fromFile.fp != NULL);
        err = Nu_FRead(pDataSource->fromFile.fp, buf, len);
        if (feof(pDataSource->fromFile.fp))
            Nu_ReportError(NU_NILBLOB, err, "EOF hit unexpectedly");
        return err;

    case kNuDataSourceFromFP:
        err = Nu_FRead(pDataSource->fromFP.fp, buf, len);
        if (feof(pDataSource->fromFP.fp))
            Nu_ReportError(NU_NILBLOB, err, "EOF hit unexpectedly");
        return err;

    case kNuDataSourceFromBuffer:
        if ((long)len > pDataSource->fromBuffer.curDataLen) {
            /* buffer underrun */
            return kNuErrBufferUnderrun;
        }
        memcpy(buf, 
            pDataSource->fromBuffer.buffer + pDataSource->fromBuffer.curOffset,
            len);
        pDataSource->fromBuffer.curOffset += len;
        pDataSource->fromBuffer.curDataLen -= len;
        return kNuErrNone;

    default:
        Assert(false);
        return kNuErrInternal;
    }
}


/*
 * Rewind a data source to the start of its input.
 */
NuError Nu_DataSourceRewind(NuDataSource* pDataSource)
{
    NuError err;

    Assert(pDataSource != NULL);

    switch (pDataSource->sourceType) {
    case kNuDataSourceFromFile:
        Assert(pDataSource->fromFile.fp != NULL);
        err = Nu_FSeek(pDataSource->fromFile.fp, 0, SEEK_SET);
        break; /* fall through with error */
    case kNuDataSourceFromFP:
        err = Nu_FSeek(pDataSource->fromFP.fp, pDataSource->fromFP.offset,
                SEEK_SET);
        break; /* fall through with error */
    case kNuDataSourceFromBuffer:
        pDataSource->fromBuffer.curOffset = pDataSource->fromBuffer.offset;
        pDataSource->fromBuffer.curDataLen = pDataSource->common.dataLen;
        err = kNuErrNone;
        break;
    default:
        Assert(false);
        err = kNuErrInternal;
    }

    return err;
}


/*
 * ===========================================================================
 *      NuDataSink
 * ===========================================================================
 */

/*
 * Allocate a new DataSink structure.
 */
static NuError Nu_DataSinkNew(NuDataSink** ppDataSink)
{
    Assert(ppDataSink != NULL);

    *ppDataSink = Nu_Malloc(NULL, sizeof(**ppDataSink));
    if (*ppDataSink == NULL)
        return kNuErrMalloc;

    (*ppDataSink)->sinkType = kNuDataSinkUnknown;

    return kNuErrNone;
}


/*
 * Free a data sink structure, and any type-specific elements.
 */
NuError Nu_DataSinkFree(NuDataSink* pDataSink)
{
    if (pDataSink == NULL)
        return kNuErrNone;

    switch (pDataSink->sinkType) {
    case kNuDataSinkToFile:
        Nu_DataSinkFile_Close(pDataSink);
        Nu_Free(NULL, pDataSink->toFile.pathnameUNI);
        break;
    case kNuDataSinkToFP:
        break;
    case kNuDataSinkToBuffer:
        break;
    case kNuDataSinkToVoid:
        break;
    case kNuDataSinkUnknown:
        break;
    default:
        Assert(0);
        return kNuErrInternal;
    }

    Nu_Free(NULL, pDataSink);
    return kNuErrNone;
}


/*
 * Create a data sink for an unopened file.
 */
NuError Nu_DataSinkFile_New(Boolean doExpand, NuValue convertEOL,
    const UNICHAR* pathnameUNI, UNICHAR fssep, NuDataSink** ppDataSink)
{
    NuError err;

    if ((doExpand != true && doExpand != false) ||
        (convertEOL != kNuConvertOff && convertEOL != kNuConvertOn &&
         convertEOL != kNuConvertAuto) ||
        pathnameUNI == NULL ||
        fssep == 0 ||
        ppDataSink == NULL)
    {
        return kNuErrInvalidArg;
    }

    err = Nu_DataSinkNew(ppDataSink);
    BailErrorQuiet(err);

    (*ppDataSink)->common.sinkType = kNuDataSinkToFile;
    (*ppDataSink)->common.doExpand = doExpand;
    if (doExpand)
        (*ppDataSink)->common.convertEOL = convertEOL;
    else
        (*ppDataSink)->common.convertEOL = kNuConvertOff;
    (*ppDataSink)->common.outCount = 0;
    (*ppDataSink)->toFile.pathnameUNI = strdup(pathnameUNI);
    (*ppDataSink)->toFile.fssep = fssep;

    (*ppDataSink)->toFile.fp = NULL;

bail:
    return err;
}


/*
 * Create a data sink based on a file pointer.
 */
NuError Nu_DataSinkFP_New(Boolean doExpand, NuValue convertEOL, FILE* fp,
    NuDataSink** ppDataSink)
{
    NuError err;

    if ((doExpand != true && doExpand != false) ||
        (convertEOL != kNuConvertOff && convertEOL != kNuConvertOn &&
         convertEOL != kNuConvertAuto) ||
        fp == NULL ||
        ppDataSink == NULL)
    {
        return kNuErrInvalidArg;
    }

    err = Nu_DataSinkNew(ppDataSink);
    BailErrorQuiet(err);

    (*ppDataSink)->common.sinkType = kNuDataSinkToFP;
    (*ppDataSink)->common.doExpand = doExpand;
    if (doExpand)
        (*ppDataSink)->common.convertEOL = convertEOL;
    else
        (*ppDataSink)->common.convertEOL = kNuConvertOff;
    (*ppDataSink)->common.outCount = 0;
    (*ppDataSink)->toFP.fp = fp;

bail:
    return err;
}


/*
 * Create a data sink for a buffer in memory.
 */
NuError Nu_DataSinkBuffer_New(Boolean doExpand, NuValue convertEOL,
    uint8_t* buffer, uint32_t bufLen, NuDataSink** ppDataSink)
{
    NuError err;

    if ((doExpand != true && doExpand != false) ||
        (convertEOL != kNuConvertOff && convertEOL != kNuConvertOn &&
         convertEOL != kNuConvertAuto) ||
        buffer == NULL ||
        bufLen == 0 ||
        ppDataSink == NULL)
    {
        return kNuErrInvalidArg;
    }

    err = Nu_DataSinkNew(ppDataSink);
    BailErrorQuiet(err);

    (*ppDataSink)->common.sinkType = kNuDataSinkToBuffer;
    (*ppDataSink)->common.doExpand = doExpand;
    if (doExpand)
        (*ppDataSink)->common.convertEOL = convertEOL;
    else
        (*ppDataSink)->common.convertEOL = kNuConvertOff;
    (*ppDataSink)->common.convertEOL = convertEOL;
    (*ppDataSink)->common.outCount = 0;
    (*ppDataSink)->toBuffer.buffer = buffer;
    (*ppDataSink)->toBuffer.bufLen = bufLen;
    (*ppDataSink)->toBuffer.stickyErr = kNuErrNone;

bail:
    return err;
}


/*
 * Create a data sink that goes nowhere.
 */
NuError Nu_DataSinkVoid_New(Boolean doExpand, NuValue convertEOL,
    NuDataSink** ppDataSink)
{
    NuError err;

    Assert(doExpand == true || doExpand == false);
    Assert(ppDataSink != NULL);

    err = Nu_DataSinkNew(ppDataSink);
    BailErrorQuiet(err);

    (*ppDataSink)->common.sinkType = kNuDataSinkToVoid;
    (*ppDataSink)->common.doExpand = doExpand;
    (*ppDataSink)->common.convertEOL = convertEOL;
    (*ppDataSink)->common.outCount = 0;

bail:
    return err;
}


/*
 * Get the type of a NuDataSink.
 */
NuDataSinkType Nu_DataSinkGetType(const NuDataSink* pDataSink)
{
    Assert(pDataSink != NULL);
    return pDataSink->sinkType;
}


/*
 * Return the "doExpand" parameter from any kind of sink.
 */
Boolean Nu_DataSinkGetDoExpand(const NuDataSink* pDataSink)
{
    return pDataSink->common.doExpand;
}

/*
 * Return the "convertEOL" parameter from any kind of sink.
 */
NuValue Nu_DataSinkGetConvertEOL(const NuDataSink* pDataSink)
{
    return pDataSink->common.convertEOL;
}

/*
 * Return the #of bytes written to the sink.
 */
uint32_t Nu_DataSinkGetOutCount(const NuDataSink* pDataSink)
{
    return pDataSink->common.outCount;
}


/*
 * Get "pathname" from a to-file sink.  Returned string is UTF-8.
 */
const char* Nu_DataSinkFile_GetPathname(const NuDataSink* pDataSink)
{
    Assert(pDataSink != NULL);
    Assert(pDataSink->sinkType == kNuDataSinkToFile);

    return pDataSink->toFile.pathnameUNI;
}

/*
 * Get "fssep" from a to-file sink.
 */
UNICHAR Nu_DataSinkFile_GetFssep(const NuDataSink* pDataSink)
{
    Assert(pDataSink != NULL);
    Assert(pDataSink->sinkType == kNuDataSinkToFile);

    return pDataSink->toFile.fssep;
}

/*
 * Get the "fp" for a file sink.
 */
FILE* Nu_DataSinkFile_GetFP(const NuDataSink* pDataSink)
{
    Assert(pDataSink != NULL);
    Assert(pDataSink->sinkType == kNuDataSinkToFile);

    return pDataSink->toFile.fp;
}

/*
 * Set the "fp" for a file sink.
 */
void Nu_DataSinkFile_SetFP(NuDataSink* pDataSink, FILE* fp)
{
    Assert(pDataSink != NULL);
    Assert(pDataSink->sinkType == kNuDataSinkToFile);

    pDataSink->toFile.fp = fp;
}

/*
 * Close a to-file sink.
 */
void Nu_DataSinkFile_Close(NuDataSink* pDataSink)
{
    Assert(pDataSink != NULL);

    if (pDataSink->toFile.fp != NULL) {
        fclose(pDataSink->toFile.fp);
        pDataSink->toFile.fp = NULL;
    }
}


/*
 * Write a block of data to a DataSink.
 */
NuError Nu_DataSinkPutBlock(NuDataSink* pDataSink, const uint8_t* buf,
    uint32_t len)
{
    NuError err;

    Assert(pDataSink != NULL);
    Assert(buf != NULL);
    Assert(len > 0);

    switch (pDataSink->sinkType) {
    case kNuDataSinkToFile:
        Assert(pDataSink->toFile.fp != NULL);
        err = Nu_FWrite(pDataSink->toFile.fp, buf, len);
        if (err != kNuErrNone)
            return err;
        break;
    case kNuDataSinkToFP:
        Assert(pDataSink->toFP.fp != NULL);
        err = Nu_FWrite(pDataSink->toFP.fp, buf, len);
        if (err != kNuErrNone)
            return err;
        break;
    case kNuDataSinkToBuffer:
        if (len > pDataSink->toBuffer.bufLen) {
            /* buffer overrun; set a "sticky" error, like FILE* does */
            err = kNuErrBufferOverrun;
            pDataSink->toBuffer.stickyErr = err;
            return err;
        }
        memcpy(pDataSink->toBuffer.buffer, buf, len);
        pDataSink->toBuffer.buffer += len;
        pDataSink->toBuffer.bufLen -= len;
        break;
    case kNuDataSinkToVoid:
        /* do nothing */
        break;
    default:
        Assert(false);
        return kNuErrInternal;
    }
    pDataSink->common.outCount += len;
    return kNuErrNone;
}


/*
 * Figure out if one of our earlier writes has failed.
 */
NuError Nu_DataSinkGetError(NuDataSink* pDataSink)
{
    NuError err = kNuErrNone;

    Assert(pDataSink != NULL);

    switch (pDataSink->sinkType) {
    case kNuDataSinkToFile:
        if (ferror(pDataSink->toFile.fp))
            err = kNuErrFileWrite;
        break;
    case kNuDataSinkToFP:
        if (ferror(pDataSink->toFP.fp))
            err = kNuErrFileWrite;
        break;
    case kNuDataSinkToBuffer:
        err = pDataSink->toBuffer.stickyErr;
        break;
    case kNuDataSinkToVoid:
        /* do nothing */
        break;
    default:
        Assert(false);
        err = kNuErrInternal;
        break;
    }

    return err;
}

