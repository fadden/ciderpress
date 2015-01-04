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
static inline void Nu_SetBusy(NuArchive* pArchive)
{
    pArchive->busy = true;
}

/*
 * Clear the busy flag.
 */
static inline void Nu_ClearBusy(NuArchive* pArchive)
{
    pArchive->busy = false;
}


/*
 * Do a partial validation on NuArchive.  Some calls, such as GetExtraData,
 * can be made during callback functions when the archive isn't fully
 * consistent.
 */
static NuError Nu_PartiallyValidateNuArchive(const NuArchive* pArchive)
{
    if (pArchive == NULL)
        return kNuErrInvalidArg;

    if (pArchive->structMagic != kNuArchiveStructMagic)
        return kNuErrBadStruct;

    return kNuErrNone;
}

/*
 * Validate the NuArchive* argument passed in to us.
 */
static NuError Nu_ValidateNuArchive(const NuArchive* pArchive)
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
            Assert(Nu_RecordSet_GetListHead(&pArchive->origRecordSet) != NULL);
        Assert(Nu_RecordSet_GetNumRecords(&pArchive->origRecordSet) ==
               pArchive->masterHeader.mhTotalRecords);
    } else {
        Assert(Nu_RecordSet_GetListHead(&pArchive->origRecordSet) == NULL);
    }

    /* make sure we have open files to work with */
    Assert(pArchive->archivePathnameUNI == NULL || pArchive->archiveFp != NULL);
    if (pArchive->archivePathnameUNI != NULL && pArchive->archiveFp == NULL)
        return kNuErrInternal;
    Assert(pArchive->tmpPathnameUNI == NULL || pArchive->tmpFp != NULL);
    if (pArchive->tmpPathnameUNI != NULL && pArchive->tmpFp == NULL)
        return kNuErrInternal;

    /* further validations */

    return kNuErrNone;
}


/*
 * ===========================================================================
 *      Streaming and non-streaming read-only
 * ===========================================================================
 */

NUFXLIB_API NuError NuStreamOpenRO(FILE* infp, NuArchive** ppArchive)
{
    NuError err;

    if (infp == NULL || ppArchive == NULL)
        return kNuErrInvalidArg;

    err = Nu_StreamOpenRO(infp, (NuArchive**) ppArchive);

    return err;
}

NUFXLIB_API NuError NuContents(NuArchive* pArchive, NuCallback contentFunc)
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

NUFXLIB_API NuError NuExtract(NuArchive* pArchive)
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

NUFXLIB_API NuError NuTest(NuArchive* pArchive)
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

NUFXLIB_API NuError NuTestRecord(NuArchive* pArchive, NuRecordIdx recordIdx)
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

NUFXLIB_API NuError NuOpenRO(const UNICHAR* archivePathnameUNI,
    NuArchive** ppArchive)
{
    NuError err;

    err = Nu_OpenRO(archivePathnameUNI, (NuArchive**) ppArchive);

    return err;
}

NUFXLIB_API NuError NuExtractRecord(NuArchive* pArchive, NuRecordIdx recordIdx)
{
    NuError err;

    if ((err = Nu_ValidateNuArchive(pArchive)) == kNuErrNone) {
        Nu_SetBusy(pArchive);
        err = Nu_ExtractRecord(pArchive, recordIdx);
        Nu_ClearBusy(pArchive);
    }

    return err;
}

NUFXLIB_API NuError NuExtractThread(NuArchive* pArchive, NuThreadIdx threadIdx,
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

NUFXLIB_API NuError NuGetRecord(NuArchive* pArchive, NuRecordIdx recordIdx,
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

NUFXLIB_API NuError NuGetRecordIdxByName(NuArchive* pArchive,
    const char* nameMOR, NuRecordIdx* pRecordIdx)
{
    NuError err;

    if ((err = Nu_ValidateNuArchive(pArchive)) == kNuErrNone) {
        Nu_SetBusy(pArchive);
        err = Nu_GetRecordIdxByName(pArchive, nameMOR, pRecordIdx);
        Nu_ClearBusy(pArchive);
    }

    return err;
}

NUFXLIB_API NuError NuGetRecordIdxByPosition(NuArchive* pArchive, uint32_t position,
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

NUFXLIB_API NuError NuOpenRW(const UNICHAR* archivePathnameUNI,
    const UNICHAR* tmpPathnameUNI, uint32_t flags, NuArchive** ppArchive)
{
    NuError err;

    err = Nu_OpenRW(archivePathnameUNI, tmpPathnameUNI, flags,
            (NuArchive**) ppArchive);

    return err;
}

NUFXLIB_API NuError NuFlush(NuArchive* pArchive, uint32_t* pStatusFlags)
{
    NuError err;

    if ((err = Nu_ValidateNuArchive(pArchive)) == kNuErrNone) {
        Nu_SetBusy(pArchive);
        err = Nu_Flush(pArchive, pStatusFlags);
        Nu_ClearBusy(pArchive);
    }

    return err;
}

NUFXLIB_API NuError NuAbort(NuArchive* pArchive)
{
    NuError err;

    if ((err = Nu_ValidateNuArchive(pArchive)) == kNuErrNone) {
        Nu_SetBusy(pArchive);
        err = Nu_Abort(pArchive);
        Nu_ClearBusy(pArchive);
    }

    return err;
}

NUFXLIB_API NuError NuAddRecord(NuArchive* pArchive,
    const NuFileDetails* pFileDetails, NuRecordIdx* pRecordIdx)
{
    NuError err;

    if ((err = Nu_ValidateNuArchive(pArchive)) == kNuErrNone) {
        Nu_SetBusy(pArchive);
        err = Nu_AddRecord(pArchive, pFileDetails, pRecordIdx, NULL);
        Nu_ClearBusy(pArchive);
    }

    return err;
}

NUFXLIB_API NuError NuAddThread(NuArchive* pArchive, NuRecordIdx recordIdx,
    NuThreadID threadID, NuDataSource* pDataSource, NuThreadIdx* pThreadIdx)
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

NUFXLIB_API NuError NuAddFile(NuArchive* pArchive, const UNICHAR* pathnameUNI,
    const NuFileDetails* pFileDetails, short isFromRsrcFork,
    NuRecordIdx* pRecordIdx)
{
    NuError err;

    if ((err = Nu_ValidateNuArchive(pArchive)) == kNuErrNone) {
        Nu_SetBusy(pArchive);
        err = Nu_AddFile(pArchive, pathnameUNI, pFileDetails,
                (Boolean)(isFromRsrcFork != 0), pRecordIdx);
        Nu_ClearBusy(pArchive);
    }

    return err;
}

NUFXLIB_API NuError NuRename(NuArchive* pArchive, NuRecordIdx recordIdx,
    const char* pathnameMOR, char fssep)
{
    NuError err;

    if ((err = Nu_ValidateNuArchive(pArchive)) == kNuErrNone) {
        Nu_SetBusy(pArchive);
        err = Nu_Rename(pArchive, recordIdx, pathnameMOR, fssep);
        Nu_ClearBusy(pArchive);
    }

    return err;
}


NUFXLIB_API NuError NuSetRecordAttr(NuArchive* pArchive, NuRecordIdx recordIdx,
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

NUFXLIB_API NuError NuUpdatePresizedThread(NuArchive* pArchive,
    NuThreadIdx threadIdx, NuDataSource* pDataSource, int32_t* pMaxLen)
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

NUFXLIB_API NuError NuDelete(NuArchive* pArchive)
{
    NuError err;

    if ((err = Nu_ValidateNuArchive(pArchive)) == kNuErrNone) {
        Nu_SetBusy(pArchive);
        err = Nu_Delete(pArchive);
        Nu_ClearBusy(pArchive);
    }

    return err;
}

NUFXLIB_API NuError NuDeleteRecord(NuArchive* pArchive, NuRecordIdx recordIdx)
{
    NuError err;

    if ((err = Nu_ValidateNuArchive(pArchive)) == kNuErrNone) {
        Nu_SetBusy(pArchive);
        err = Nu_DeleteRecord(pArchive, recordIdx);
        Nu_ClearBusy(pArchive);
    }

    return err;
}

NUFXLIB_API NuError NuDeleteThread(NuArchive* pArchive, NuThreadIdx threadIdx)
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

NUFXLIB_API NuError NuClose(NuArchive* pArchive)
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

NUFXLIB_API NuError NuGetMasterHeader(NuArchive* pArchive,
    const NuMasterHeader** ppMasterHeader)
{
    NuError err;

    if ((err = Nu_ValidateNuArchive(pArchive)) == kNuErrNone)
        err = Nu_GetMasterHeader(pArchive, ppMasterHeader);

    return err;
}

NUFXLIB_API NuError NuGetExtraData(NuArchive* pArchive, void** ppData)
{
    NuError err;

    if (ppData == NULL)
        return kNuErrInvalidArg;
    if ((err = Nu_PartiallyValidateNuArchive(pArchive)) == kNuErrNone)
        *ppData = pArchive->extraData;

    return err;
}

NUFXLIB_API NuError NuSetExtraData(NuArchive* pArchive, void* pData)
{
    NuError err;

    if ((err = Nu_PartiallyValidateNuArchive(pArchive)) == kNuErrNone)
        pArchive->extraData = pData;

    return err;
}

NUFXLIB_API NuError NuGetValue(NuArchive* pArchive, NuValueID ident,
    NuValue* pValue)
{
    NuError err;

    if ((err = Nu_PartiallyValidateNuArchive(pArchive)) == kNuErrNone)
        return Nu_GetValue(pArchive, ident, pValue);

    return err;
}

NUFXLIB_API NuError NuSetValue(NuArchive* pArchive, NuValueID ident,
    NuValue value)
{
    NuError err;

    if ((err = Nu_PartiallyValidateNuArchive(pArchive)) == kNuErrNone)
        return Nu_SetValue(pArchive, ident, value);

    return err;
}

NUFXLIB_API NuError NuGetAttr(NuArchive* pArchive, NuAttrID ident,
    NuAttr* pAttr)
{
    NuError err;

    if ((err = Nu_PartiallyValidateNuArchive(pArchive)) == kNuErrNone)
        return Nu_GetAttr(pArchive, ident, pAttr);

    return err;
}

NUFXLIB_API NuError NuDebugDumpArchive(NuArchive* pArchive)
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

NUFXLIB_API NuError NuCreateDataSourceForFile(NuThreadFormat threadFormat,
    uint32_t otherLen, const UNICHAR* pathnameUNI, short isFromRsrcFork,
    NuDataSource** ppDataSource)
{
    return Nu_DataSourceFile_New(threadFormat, otherLen,
            pathnameUNI, (Boolean)(isFromRsrcFork != 0), ppDataSource);
}

NUFXLIB_API NuError NuCreateDataSourceForFP(NuThreadFormat threadFormat,
    uint32_t otherLen, FILE* fp, long offset, long length,
    NuCallback fcloseFunc, NuDataSource** ppDataSource)
{
    return Nu_DataSourceFP_New(threadFormat, otherLen,
            fp, offset, length, fcloseFunc, ppDataSource);
}

NUFXLIB_API NuError NuCreateDataSourceForBuffer(NuThreadFormat threadFormat,
    uint32_t otherLen, const uint8_t* buffer, long offset,
    long length, NuCallback freeFunc, NuDataSource** ppDataSource)
{
    return Nu_DataSourceBuffer_New(threadFormat, otherLen,
            buffer, offset, length, freeFunc, ppDataSource);
}

NUFXLIB_API NuError NuFreeDataSource(NuDataSource* pDataSource)
{
    return Nu_DataSourceFree(pDataSource);
}

NUFXLIB_API NuError NuDataSourceSetRawCrc(NuDataSource* pDataSource,
    uint16_t crc)
{
    if (pDataSource == NULL)
        return kNuErrInvalidArg;
    Nu_DataSourceSetRawCrc(pDataSource, crc);
    return kNuErrNone;
}

NUFXLIB_API NuError NuCreateDataSinkForFile(short doExpand, NuValue convertEOL,
    const UNICHAR* pathnameUNI, UNICHAR fssep, NuDataSink** ppDataSink)
{
    return Nu_DataSinkFile_New((Boolean)(doExpand != 0), convertEOL,
            pathnameUNI, fssep, ppDataSink);
}

NUFXLIB_API NuError NuCreateDataSinkForFP(short doExpand, NuValue convertEOL,
    FILE* fp, NuDataSink** ppDataSink)
{
    return Nu_DataSinkFP_New((Boolean)(doExpand != 0), convertEOL, fp,
            ppDataSink);
}

NUFXLIB_API NuError NuCreateDataSinkForBuffer(short doExpand,
    NuValue convertEOL, uint8_t* buffer, uint32_t bufLen,
    NuDataSink** ppDataSink)
{
    return Nu_DataSinkBuffer_New((Boolean)(doExpand != 0), convertEOL, buffer,
            bufLen, ppDataSink);
}

NUFXLIB_API NuError NuFreeDataSink(NuDataSink* pDataSink)
{
    return Nu_DataSinkFree(pDataSink);
}

NUFXLIB_API NuError NuDataSinkGetOutCount(NuDataSink* pDataSink,
    uint32_t* pOutCount)
{
    if (pDataSink == NULL || pOutCount == NULL)
        return kNuErrInvalidArg;

    *pOutCount = Nu_DataSinkGetOutCount(pDataSink);
    return kNuErrNone;
}


/*
 * ===========================================================================
 *      Non-archive operations
 * ===========================================================================
 */

NUFXLIB_API const char* NuStrError(NuError err)
{
    return Nu_StrError(err);
}

NUFXLIB_API NuError NuGetVersion(int32_t* pMajorVersion, int32_t* pMinorVersion,
    int32_t* pBugVersion, const char** ppBuildDate, const char** ppBuildFlags)
{
    return Nu_GetVersion(pMajorVersion, pMinorVersion, pBugVersion,
            ppBuildDate, ppBuildFlags);
}

NUFXLIB_API NuError NuTestFeature(NuFeature feature)
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

NUFXLIB_API void NuRecordCopyAttr(NuRecordAttr* pRecordAttr,
    const NuRecord* pRecord)
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

NUFXLIB_API NuError NuRecordCopyThreads(const NuRecord* pNuRecord,
    NuThread** ppThreads)
{
    if (pNuRecord == NULL || ppThreads == NULL)
        return kNuErrInvalidArg;

    Assert(pNuRecord->pThreads != NULL);

    *ppThreads = Nu_Malloc(NULL, pNuRecord->recTotalThreads * sizeof(NuThread));
    if (*ppThreads == NULL)
        return kNuErrMalloc;

    memcpy(*ppThreads, pNuRecord->pThreads,
        pNuRecord->recTotalThreads * sizeof(NuThread));

    return kNuErrNone;
}

NUFXLIB_API uint32_t NuRecordGetNumThreads(const NuRecord* pNuRecord)
{
    if (pNuRecord == NULL)
        return -1;

    return pNuRecord->recTotalThreads;
}

NUFXLIB_API const NuThread* NuThreadGetByIdx(const NuThread* pNuThread,
    int32_t idx)
{
    if (pNuThread == NULL)
        return NULL;
    return &pNuThread[idx];     /* can't range-check here */
}

NUFXLIB_API short NuIsPresizedThreadID(NuThreadID threadID)
{
    return Nu_IsPresizedThreadID(threadID);
}

NUFXLIB_API size_t NuConvertMORToUNI(const char* stringMOR,
    UNICHAR* bufUNI, size_t bufSize)
{
    return Nu_ConvertMORToUNI(stringMOR, bufUNI, bufSize);
}

NUFXLIB_API size_t NuConvertUNIToMOR(const UNICHAR* stringUNI,
    char* bufMOR, size_t bufSize)
{
    return Nu_ConvertUNIToMOR(stringUNI, bufMOR, bufSize);
}


/*
 * ===========================================================================
 *      Callback setters
 * ===========================================================================
 */

NUFXLIB_API NuCallback NuSetSelectionFilter(NuArchive* pArchive,
    NuCallback filterFunc)
{
    NuError err;
    NuCallback oldFunc = kNuInvalidCallback;

    /*Assert(!((uint32_t)filterFunc % 4));*/

    if ((err = Nu_ValidateNuArchive(pArchive)) == kNuErrNone) {
        oldFunc = pArchive->selectionFilterFunc;
        pArchive->selectionFilterFunc = filterFunc;
    }

    return oldFunc;
}

NUFXLIB_API NuCallback NuSetOutputPathnameFilter(NuArchive* pArchive,
    NuCallback filterFunc)
{
    NuError err;
    NuCallback oldFunc = kNuInvalidCallback;

    /*Assert(!((uint32_t)filterFunc % 4));*/

    if ((err = Nu_ValidateNuArchive(pArchive)) == kNuErrNone) {
        oldFunc = pArchive->outputPathnameFunc;
        pArchive->outputPathnameFunc = filterFunc;
    }

    return oldFunc;
}

NUFXLIB_API NuCallback NuSetProgressUpdater(NuArchive* pArchive,
    NuCallback updateFunc)
{
    NuError err;
    NuCallback oldFunc = kNuInvalidCallback;

    /*Assert(!((uint32_t)updateFunc % 4));*/

    if ((err = Nu_ValidateNuArchive(pArchive)) == kNuErrNone) {
        oldFunc = pArchive->progressUpdaterFunc;
        pArchive->progressUpdaterFunc = updateFunc;
    }

    return oldFunc;
}

NUFXLIB_API NuCallback NuSetErrorHandler(NuArchive* pArchive,
    NuCallback errorFunc)
{
    NuError err;
    NuCallback oldFunc = kNuInvalidCallback;

    /*Assert(!((uint32_t)errorFunc % 4));*/

    if ((err = Nu_ValidateNuArchive(pArchive)) == kNuErrNone) {
        oldFunc = pArchive->errorHandlerFunc;
        pArchive->errorHandlerFunc = errorFunc;
    }

    return oldFunc;
}

NUFXLIB_API NuCallback NuSetErrorMessageHandler(NuArchive* pArchive,
    NuCallback messageHandlerFunc)
{
    NuError err;
    NuCallback oldFunc = kNuInvalidCallback;

    /*Assert(!((uint32_t)messageHandlerFunc % 4));*/

    if ((err = Nu_ValidateNuArchive(pArchive)) == kNuErrNone) {
        oldFunc = pArchive->messageHandlerFunc;
        pArchive->messageHandlerFunc = messageHandlerFunc;
    }

    return oldFunc;
}

NUFXLIB_API NuCallback NuSetGlobalErrorMessageHandler(NuCallback messageHandlerFunc)
{
    NuCallback oldFunc = kNuInvalidCallback;
    /*Assert(!((uint32_t)messageHandlerFunc % 4));*/

    oldFunc = gNuGlobalErrorMessageHandler;
    gNuGlobalErrorMessageHandler = messageHandlerFunc;
    return oldFunc;
}

