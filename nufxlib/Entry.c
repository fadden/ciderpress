/*
 * NuFX archive manipulation library
 * Copyright (C) 2000-2007 by Andy McFadden, All Rights Reserved.
 * This is free software; you can redistribute it and/or modify it under the
 * terms of the BSD License, see the file COPYING-LIB.
 *
 * All external entry points.
 */
#include "NufxLibPriv.h"


/*
 * ===========================================================================
 *      Misc utils
 * ===========================================================================
 */

/*
 * Set the busy flag.
 *
 * The busy flag is intended to prevent the caller from executing illegal
 * operations while inside a callback function.  It is NOT intended to
 * allow concurrent access to the same archive from multiple threads, so
 * it does not follow all sorts of crazy semaphore semantics.  If you
 * have the need, go ahead and fix it.
 */
static inline void
Nu_SetBusy(NuArchive* pArchive)
{
    pArchive->busy = true;
}

/*
 * Clear the busy flag.
 */
static inline void
Nu_ClearBusy(NuArchive* pArchive)
{
    pArchive->busy = false;
}


/*
 * Do a partial validation on NuArchive.  Some calls, such as GetExtraData,
 * can be made during callback functions when the archive isn't fully
 * consistent.
 */
static NuError
Nu_PartiallyValidateNuArchive(const NuArchive* pArchive)
{
    if (pArchive == nil)
        return kNuErrInvalidArg;

    pArchive =  pArchive;
    if (pArchive->structMagic != kNuArchiveStructMagic)
        return kNuErrBadStruct;

    return kNuErrNone;
}

/*
 * Validate the NuArchive* argument passed in to us.
 */
static NuError
Nu_ValidateNuArchive(const NuArchive* pArchive)
{
    NuError err;

    err = Nu_PartiallyValidateNuArchive(pArchive);
    if (err != kNuErrNone)
        return err;

    /* explicitly block reentrant calls */
    if (pArchive->busy)
        return kNuErrBusy;

    /* make sure the TOC state is consistent */
    if (pArchive->haveToc) {
        if (pArchive->masterHeader.mhTotalRecords != 0)
            Assert(Nu_RecordSet_GetListHead(&pArchive->origRecordSet) != nil);
        Assert(Nu_RecordSet_GetNumRecords(&pArchive->origRecordSet) ==
               pArchive->masterHeader.mhTotalRecords);
    } else {
        Assert(Nu_RecordSet_GetListHead(&pArchive->origRecordSet) == nil);
    }

    /* make sure we have open files to work with */
    Assert(pArchive->archivePathname == nil || pArchive->archiveFp != nil);
    if (pArchive->archivePathname != nil && pArchive->archiveFp == nil)
        return kNuErrInternal;
    Assert(pArchive->tmpPathname == nil || pArchive->tmpFp != nil);
    if (pArchive->tmpPathname != nil && pArchive->tmpFp == nil)
        return kNuErrInternal;

    /* further validations */

    return kNuErrNone;
}


/*
 * ===========================================================================
 *      Streaming and non-streaming read-only
 * ===========================================================================
 */

NUFXLIB_API NuError
NuStreamOpenRO(FILE* infp, NuArchive** ppArchive)
{
    NuError err;

    if (infp == nil || ppArchive == nil)
        return kNuErrInvalidArg;

    err = Nu_StreamOpenRO(infp, (NuArchive**) ppArchive);

    return err;
}

NUFXLIB_API NuError
NuContents(NuArchive* pArchive, NuCallback contentFunc)
{
    NuError err;

    if ((err = Nu_ValidateNuArchive(pArchive)) == kNuErrNone) {
        Nu_SetBusy(pArchive);
        if (Nu_IsStreaming(pArchive))
            err = Nu_StreamContents(pArchive, contentFunc);
        else
            err = Nu_Contents(pArchive, contentFunc);
        Nu_ClearBusy(pArchive);
    }

    return err;
}

NUFXLIB_API NuError
NuExtract(NuArchive* pArchive)
{
    NuError err;

    if ((err = Nu_ValidateNuArchive(pArchive)) == kNuErrNone) {
        Nu_SetBusy(pArchive);
        if (Nu_IsStreaming(pArchive))
            err = Nu_StreamExtract(pArchive);
        else
            err = Nu_Extract(pArchive);
        Nu_ClearBusy(pArchive);
    }

    return err;
}

NUFXLIB_API NuError
NuTest(NuArchive* pArchive)
{
    NuError err;

    if ((err = Nu_ValidateNuArchive(pArchive)) == kNuErrNone) {
        Nu_SetBusy(pArchive);
        if (Nu_IsStreaming(pArchive))
            err = Nu_StreamTest(pArchive);
        else
            err = Nu_Test(pArchive);
        Nu_ClearBusy(pArchive);
    }

    return err;
}

NUFXLIB_API NuError
NuTestRecord(NuArchive* pArchive, NuRecordIdx recordIdx)
{
    NuError err;

    if ((err = Nu_ValidateNuArchive(pArchive)) == kNuErrNone) {
        Nu_SetBusy(pArchive);
        err = Nu_TestRecord(pArchive, recordIdx);
        Nu_ClearBusy(pArchive);
    }

    return err;
}


/*
 * ===========================================================================
 *      Strictly non-streaming read-only
 * ===========================================================================
 */

NUFXLIB_API NuError
NuOpenRO(const char* filename, NuArchive** ppArchive)
{
    NuError err;

    err = Nu_OpenRO(filename, (NuArchive**) ppArchive);

    return err;
}

NUFXLIB_API NuError
NuExtractRecord(NuArchive* pArchive, NuRecordIdx recordIdx)
{
    NuError err;

    if ((err = Nu_ValidateNuArchive(pArchive)) == kNuErrNone) {
        Nu_SetBusy(pArchive);
        err = Nu_ExtractRecord(pArchive, recordIdx);
        Nu_ClearBusy(pArchive);
    }

    return err;
}

NUFXLIB_API NuError
NuExtractThread(NuArchive* pArchive, NuThreadIdx threadIdx,
    NuDataSink* pDataSink)
{
    NuError err;

    if ((err = Nu_ValidateNuArchive(pArchive)) == kNuErrNone) {
        Nu_SetBusy(pArchive);
        err = Nu_ExtractThread(pArchive, threadIdx, pDataSink);
        Nu_ClearBusy(pArchive);
    }

    return err;
}

NUFXLIB_API NuError
NuGetRecord(NuArchive* pArchive, NuRecordIdx recordIdx,
    const NuRecord** ppRecord)
{
    NuError err;

    if ((err = Nu_ValidateNuArchive(pArchive)) == kNuErrNone) {
        Nu_SetBusy(pArchive);
        err = Nu_GetRecord(pArchive, recordIdx, ppRecord);
        Nu_ClearBusy(pArchive);
    }

    return err;
}

NUFXLIB_API NuError
NuGetRecordIdxByName(NuArchive* pArchive, const char* name,
    NuRecordIdx* pRecordIdx)
{
    NuError err;

    if ((err = Nu_ValidateNuArchive(pArchive)) == kNuErrNone) {
        Nu_SetBusy(pArchive);
        err = Nu_GetRecordIdxByName(pArchive, name, pRecordIdx);
        Nu_ClearBusy(pArchive);
    }

    return err;
}

NUFXLIB_API NuError
NuGetRecordIdxByPosition(NuArchive* pArchive, unsigned long position,
    NuRecordIdx* pRecordIdx)
{
    NuError err;

    if ((err = Nu_ValidateNuArchive(pArchive)) == kNuErrNone) {
        Nu_SetBusy(pArchive);
        err = Nu_GetRecordIdxByPosition(pArchive, position, pRecordIdx);
        Nu_ClearBusy(pArchive);
    }

    return err;
}


/*
 * ===========================================================================
 *      Read/Write
 * ===========================================================================
 */

NUFXLIB_API NuError
NuOpenRW(const char* archivePathname, const char* tmpPathname,
    unsigned long flags, NuArchive** ppArchive)
{
    NuError err;

    err = Nu_OpenRW(archivePathname, tmpPathname, flags,
            (NuArchive**) ppArchive);

    return err;
}

NUFXLIB_API NuError
NuFlush(NuArchive* pArchive, long* pStatusFlags)
{
    NuError err;

    if ((err = Nu_ValidateNuArchive(pArchive)) == kNuErrNone) {
        Nu_SetBusy(pArchive);
        err = Nu_Flush(pArchive, pStatusFlags);
        Nu_ClearBusy(pArchive);
    }

    return err;
}

NUFXLIB_API NuError
NuAbort(NuArchive* pArchive)
{
    NuError err;

    if ((err = Nu_ValidateNuArchive(pArchive)) == kNuErrNone) {
        Nu_SetBusy(pArchive);
        err = Nu_Abort(pArchive);
        Nu_ClearBusy(pArchive);
    }

    return err;
}

NUFXLIB_API NuError
NuAddRecord(NuArchive* pArchive, const NuFileDetails* pFileDetails,
    NuRecordIdx* pRecordIdx)
{
    NuError err;

    if ((err = Nu_ValidateNuArchive(pArchive)) == kNuErrNone) {
        Nu_SetBusy(pArchive);
        err = Nu_AddRecord(pArchive, pFileDetails, pRecordIdx, nil);
        Nu_ClearBusy(pArchive);
    }

    return err;
}

NUFXLIB_API NuError
NuAddThread(NuArchive* pArchive, NuRecordIdx recordIdx, NuThreadID threadID,
    NuDataSource* pDataSource, NuThreadIdx* pThreadIdx)
{
    NuError err;

    if ((err = Nu_ValidateNuArchive(pArchive)) == kNuErrNone) {
        Nu_SetBusy(pArchive);
        err = Nu_AddThread(pArchive, recordIdx, threadID,
                pDataSource, pThreadIdx);
        Nu_ClearBusy(pArchive);
    }

    return err;
}

NUFXLIB_API NuError
NuAddFile(NuArchive* pArchive, const char* pathname,
    const NuFileDetails* pFileDetails, short isFromRsrcFork,
    NuRecordIdx* pRecordIdx)
{
    NuError err;

    if ((err = Nu_ValidateNuArchive(pArchive)) == kNuErrNone) {
        Nu_SetBusy(pArchive);
        err = Nu_AddFile(pArchive, pathname, pFileDetails,
                (Boolean)(isFromRsrcFork != 0), pRecordIdx);
        Nu_ClearBusy(pArchive);
    }

    return err;
}

NUFXLIB_API NuError
NuRename(NuArchive* pArchive, NuRecordIdx recordIdx, const char* pathname,
    char fssep)
{
    NuError err;

    if ((err = Nu_ValidateNuArchive(pArchive)) == kNuErrNone) {
        Nu_SetBusy(pArchive);
        err = Nu_Rename(pArchive, recordIdx, pathname, fssep);
        Nu_ClearBusy(pArchive);
    }

    return err;
}


NUFXLIB_API NuError
NuSetRecordAttr(NuArchive* pArchive, NuRecordIdx recordIdx,
    const NuRecordAttr* pRecordAttr)
{
    NuError err;

    if ((err = Nu_ValidateNuArchive(pArchive)) == kNuErrNone) {
        Nu_SetBusy(pArchive);
        err = Nu_SetRecordAttr(pArchive, recordIdx, pRecordAttr);
        Nu_ClearBusy(pArchive);
    }

    return err;
}

NUFXLIB_API NuError
NuUpdatePresizedThread(NuArchive* pArchive, NuThreadIdx threadIdx,
    NuDataSource* pDataSource, long* pMaxLen)
{
    NuError err;

    if ((err = Nu_ValidateNuArchive(pArchive)) == kNuErrNone) {
        Nu_SetBusy(pArchive);
        err = Nu_UpdatePresizedThread(pArchive, threadIdx,
                pDataSource, pMaxLen);
        Nu_ClearBusy(pArchive);
    }

    return err;
}

NUFXLIB_API NuError
NuDelete(NuArchive* pArchive)
{
    NuError err;

    if ((err = Nu_ValidateNuArchive(pArchive)) == kNuErrNone) {
        Nu_SetBusy(pArchive);
        err = Nu_Delete(pArchive);
        Nu_ClearBusy(pArchive);
    }

    return err;
}

NUFXLIB_API NuError
NuDeleteRecord(NuArchive* pArchive, NuRecordIdx recordIdx)
{
    NuError err;

    if ((err = Nu_ValidateNuArchive(pArchive)) == kNuErrNone) {
        Nu_SetBusy(pArchive);
        err = Nu_DeleteRecord(pArchive, recordIdx);
        Nu_ClearBusy(pArchive);
    }

    return err;
}

NUFXLIB_API NuError
NuDeleteThread(NuArchive* pArchive, NuThreadIdx threadIdx)
{
    NuError err;

    if ((err = Nu_ValidateNuArchive(pArchive)) == kNuErrNone) {
        Nu_SetBusy(pArchive);
        err = Nu_DeleteThread(pArchive, threadIdx);
        Nu_ClearBusy(pArchive);
    }

    return err;
}


/*
 * ===========================================================================
 *      General interfaces
 * ===========================================================================
 */

NUFXLIB_API NuError
NuClose(NuArchive* pArchive)
{
    NuError err;

    if ((err = Nu_ValidateNuArchive(pArchive)) == kNuErrNone) {
        Nu_SetBusy(pArchive);
        err = Nu_Close(pArchive);
        /* on success, pArchive has been freed */
        if (err != kNuErrNone)
            Nu_ClearBusy(pArchive);
    }

    return err;
}

NUFXLIB_API NuError
NuGetMasterHeader(NuArchive* pArchive, const NuMasterHeader** ppMasterHeader)
{
    NuError err;

    if ((err = Nu_ValidateNuArchive(pArchive)) == kNuErrNone)
        err = Nu_GetMasterHeader(pArchive, ppMasterHeader);

    return err;
}

NUFXLIB_API NuError
NuGetExtraData(NuArchive* pArchive, void** ppData)
{
    NuError err;

    if (ppData == nil)
        return kNuErrInvalidArg;
    if ((err = Nu_PartiallyValidateNuArchive(pArchive)) == kNuErrNone)
        *ppData = pArchive->extraData;

    return err;
}

NUFXLIB_API NuError
NuSetExtraData(NuArchive* pArchive, void* pData)
{
    NuError err;

    if ((err = Nu_PartiallyValidateNuArchive(pArchive)) == kNuErrNone)
        pArchive->extraData = pData;

    return err;
}

NUFXLIB_API NuError
NuGetValue(NuArchive* pArchive, NuValueID ident, NuValue* pValue)
{
    NuError err;

    if ((err = Nu_PartiallyValidateNuArchive(pArchive)) == kNuErrNone)
        return Nu_GetValue(pArchive, ident, pValue);

    return err;
}

NUFXLIB_API NuError
NuSetValue(NuArchive* pArchive, NuValueID ident, NuValue value)
{
    NuError err;

    if ((err = Nu_PartiallyValidateNuArchive(pArchive)) == kNuErrNone)
        return Nu_SetValue(pArchive, ident, value);

    return err;
}

NUFXLIB_API NuError
NuGetAttr(NuArchive* pArchive, NuAttrID ident, NuAttr* pAttr)
{
    NuError err;

    if ((err = Nu_PartiallyValidateNuArchive(pArchive)) == kNuErrNone)
        return Nu_GetAttr(pArchive, ident, pAttr);

    return err;
}

NUFXLIB_API NuError
NuDebugDumpArchive(NuArchive* pArchive)
{
#if defined(DEBUG_MSGS)
    /* skip validation checks for this one */
    Nu_DebugDumpAll(pArchive);
    return kNuErrNone;
#else
    /* function doesn't exist */
    return kNuErrGeneric;
#endif
}


/*
 * ===========================================================================
 *      Sources and Sinks
 * ===========================================================================
 */

NUFXLIB_API NuError
NuCreateDataSourceForFile(NuThreadFormat threadFormat,
    unsigned long otherLen, const char* pathname, short isFromRsrcFork,
    NuDataSource** ppDataSource)
{
    return Nu_DataSourceFile_New(threadFormat, otherLen,
            pathname, (Boolean)(isFromRsrcFork != 0), ppDataSource);
}

NUFXLIB_API NuError
NuCreateDataSourceForFP(NuThreadFormat threadFormat,
    unsigned long otherLen, FILE* fp, long offset, long length,
    NuCallback fcloseFunc, NuDataSource** ppDataSource)
{
    return Nu_DataSourceFP_New(threadFormat, otherLen,
            fp, offset, length, fcloseFunc, ppDataSource);
}

NUFXLIB_API NuError
NuCreateDataSourceForBuffer(NuThreadFormat threadFormat,
    unsigned long otherLen, const unsigned char* buffer, long offset,
    long length, NuCallback freeFunc, NuDataSource** ppDataSource)
{
    return Nu_DataSourceBuffer_New(threadFormat, otherLen,
            buffer, offset, length, freeFunc, ppDataSource);
}

NUFXLIB_API NuError
NuFreeDataSource(NuDataSource* pDataSource)
{
    return Nu_DataSourceFree(pDataSource);
}

NUFXLIB_API NuError
NuDataSourceSetRawCrc(NuDataSource* pDataSource, unsigned short crc)
{
    if (pDataSource == nil)
        return kNuErrInvalidArg;
    Nu_DataSourceSetRawCrc(pDataSource, crc);
    return kNuErrNone;
}

NUFXLIB_API NuError
NuCreateDataSinkForFile(short doExpand, NuValue convertEOL,
    const char* pathname, char fssep, NuDataSink** ppDataSink)
{
    return Nu_DataSinkFile_New((Boolean)(doExpand != 0), convertEOL, pathname,
            fssep, ppDataSink);
}

NUFXLIB_API NuError
NuCreateDataSinkForFP(short doExpand, NuValue convertEOL, FILE* fp,
    NuDataSink** ppDataSink)
{
    return Nu_DataSinkFP_New((Boolean)(doExpand != 0), convertEOL, fp,
            ppDataSink);
}

NUFXLIB_API NuError
NuCreateDataSinkForBuffer(short doExpand, NuValue convertEOL,
    unsigned char* buffer, unsigned long bufLen, NuDataSink** ppDataSink)
{
    return Nu_DataSinkBuffer_New((Boolean)(doExpand != 0), convertEOL, buffer,
            bufLen, ppDataSink);
}

NUFXLIB_API NuError
NuFreeDataSink(NuDataSink* pDataSink)
{
    return Nu_DataSinkFree(pDataSink);
}

NUFXLIB_API NuError
NuDataSinkGetOutCount(NuDataSink* pDataSink, ulong* pOutCount)
{
    if (pDataSink == nil || pOutCount == nil)
        return kNuErrInvalidArg;

    *pOutCount = Nu_DataSinkGetOutCount(pDataSink);
    return kNuErrNone;
}


/*
 * ===========================================================================
 *      Non-archive operations
 * ===========================================================================
 */

NUFXLIB_API const char*
NuStrError(NuError err)
{
    return Nu_StrError(err);
}

NUFXLIB_API NuError
NuGetVersion(long* pMajorVersion, long* pMinorVersion, long* pBugVersion,
    const char** ppBuildDate, const char** ppBuildFlags)
{
    return Nu_GetVersion(pMajorVersion, pMinorVersion, pBugVersion,
            ppBuildDate, ppBuildFlags);
}

NUFXLIB_API NuError
NuTestFeature(NuFeature feature)
{
    NuError err = kNuErrUnsupFeature;

    switch (feature) {
    case kNuFeatureCompressSQ:
        #ifdef ENABLE_SQ
        err = kNuErrNone;
        #endif
        break;
    case kNuFeatureCompressLZW:
        #ifdef ENABLE_LZW
        err = kNuErrNone;
        #endif
        break;
    case kNuFeatureCompressLZC:
        #ifdef ENABLE_LZC
        err = kNuErrNone;
        #endif
        break;
    case kNuFeatureCompressDeflate:
        #ifdef ENABLE_DEFLATE
        err = kNuErrNone;
        #endif
        break;
    case kNuFeatureCompressBzip2:
        #ifdef ENABLE_BZIP2
        err = kNuErrNone;
        #endif
        break;
    default:
        err = kNuErrUnknownFeature;
        break;
    }

    return err;
}

NUFXLIB_API void
NuRecordCopyAttr(NuRecordAttr* pRecordAttr, const NuRecord* pRecord)
{
    pRecordAttr->fileSysID = pRecord->recFileSysID;
    /*pRecordAttr->fileSysInfo = pRecord->recFileSysInfo;*/
    pRecordAttr->access = pRecord->recAccess;
    pRecordAttr->fileType = pRecord->recFileType;
    pRecordAttr->extraType = pRecord->recExtraType;
    pRecordAttr->createWhen = pRecord->recCreateWhen;
    pRecordAttr->modWhen = pRecord->recModWhen;
    pRecordAttr->archiveWhen = pRecord->recArchiveWhen;
}

NUFXLIB_API NuError
NuRecordCopyThreads(const NuRecord* pNuRecord, NuThread** ppThreads)
{
    if (pNuRecord == nil || ppThreads == nil)
        return kNuErrInvalidArg;

    Assert(pNuRecord->pThreads != nil);

    *ppThreads = Nu_Malloc(nil, pNuRecord->recTotalThreads * sizeof(NuThread));
    if (*ppThreads == nil)
        return kNuErrMalloc;

    memcpy(*ppThreads, pNuRecord->pThreads,
        pNuRecord->recTotalThreads * sizeof(NuThread));

    return kNuErrNone;
}

NUFXLIB_API unsigned long
NuRecordGetNumThreads(const NuRecord* pNuRecord)
{
    if (pNuRecord == nil)
        return -1;

    return pNuRecord->recTotalThreads;
}

NUFXLIB_API const NuThread*
NuThreadGetByIdx(const NuThread* pNuThread, long idx)
{
    if (pNuThread == nil)
        return nil;
    return &pNuThread[idx];     /* can't range-check here */
}

NUFXLIB_API short
NuIsPresizedThreadID(NuThreadID threadID)
{
    return Nu_IsPresizedThreadID(threadID);
}


/*
 * ===========================================================================
 *      Callback setters
 * ===========================================================================
 */

NUFXLIB_API NuCallback
NuSetSelectionFilter(NuArchive* pArchive, NuCallback filterFunc)
{
    NuError err;
    NuCallback oldFunc = kNuInvalidCallback;

    /*Assert(!((ulong)filterFunc % 4));*/

    if ((err = Nu_ValidateNuArchive(pArchive)) == kNuErrNone) {
        oldFunc = pArchive->selectionFilterFunc;
        pArchive->selectionFilterFunc = filterFunc;
    }

    return oldFunc;
}

NUFXLIB_API NuCallback
NuSetOutputPathnameFilter(NuArchive* pArchive, NuCallback filterFunc)
{
    NuError err;
    NuCallback oldFunc = kNuInvalidCallback;

    /*Assert(!((ulong)filterFunc % 4));*/

    if ((err = Nu_ValidateNuArchive(pArchive)) == kNuErrNone) {
        oldFunc = pArchive->outputPathnameFunc;
        pArchive->outputPathnameFunc = filterFunc;
    }

    return oldFunc;
}

NUFXLIB_API NuCallback
NuSetProgressUpdater(NuArchive* pArchive, NuCallback updateFunc)
{
    NuError err;
    NuCallback oldFunc = kNuInvalidCallback;

    /*Assert(!((ulong)updateFunc % 4));*/

    if ((err = Nu_ValidateNuArchive(pArchive)) == kNuErrNone) {
        oldFunc = pArchive->progressUpdaterFunc;
        pArchive->progressUpdaterFunc = updateFunc;
    }

    return oldFunc;
}

NUFXLIB_API NuCallback
NuSetErrorHandler(NuArchive* pArchive, NuCallback errorFunc)
{
    NuError err;
    NuCallback oldFunc = kNuInvalidCallback;

    /*Assert(!((ulong)errorFunc % 4));*/

    if ((err = Nu_ValidateNuArchive(pArchive)) == kNuErrNone) {
        oldFunc = pArchive->errorHandlerFunc;
        pArchive->errorHandlerFunc = errorFunc;
    }

    return oldFunc;
}

NUFXLIB_API NuCallback
NuSetErrorMessageHandler(NuArchive* pArchive, NuCallback messageHandlerFunc)
{
    NuError err;
    NuCallback oldFunc = kNuInvalidCallback;

    /*Assert(!((ulong)messageHandlerFunc % 4));*/

    if ((err = Nu_ValidateNuArchive(pArchive)) == kNuErrNone) {
        oldFunc = pArchive->messageHandlerFunc;
        pArchive->messageHandlerFunc = messageHandlerFunc;
    }

    return oldFunc;
}

NUFXLIB_API NuCallback
NuSetGlobalErrorMessageHandler(NuCallback messageHandlerFunc)
{
    NuCallback oldFunc = kNuInvalidCallback;
    /*Assert(!((ulong)messageHandlerFunc % 4));*/

    oldFunc = gNuGlobalErrorMessageHandler;
    gNuGlobalErrorMessageHandler = messageHandlerFunc;
    return oldFunc;
}

