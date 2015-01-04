/*
 * NuFX archive manipulation library
 * Copyright (C) 2000-2007 by Andy McFadden, All Rights Reserved.
 * This is free software; you can redistribute it and/or modify it under the
 * terms of the BSD License, see the file COPYING.LIB.
 *
 * Test extraction of individual threads in various ways.  The net result
 * of this is three files (out.file, out.fp, out.buf) that contain the
 * result of writing all filenames in an archive to the same data sink.
 *
 * This gathers up information on the contents of the archive via a
 * callback, and then emits all of the data at once.
 *
 * (This was originally written in C++, and converted to C after I repented.)
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "NufxLib.h"
#include "Common.h"


/*#define false   0*/
/*#define true    (!false)*/

#define kHappySize  2408


/*
 * ===========================================================================
 *      ArchiveRecord
 * ===========================================================================
 */

/*
 * Track an archive record.
 */
typedef struct ArchiveRecord {
    char*           filenameMOR;
    NuRecordIdx     recordIdx;

    long            numThreads;
    NuThread*       pThreads;

    struct ArchiveRecord*   pNext;
} ArchiveRecord;


/* 
 * Alloc a new ArchiveRecord.
 */
ArchiveRecord* ArchiveRecord_New(const NuRecord* pRecord)
{
    ArchiveRecord* pArcRec = NULL;

    pArcRec = malloc(sizeof(*pArcRec));
    if (pArcRec == NULL)
        return NULL;

    if (pRecord->filenameMOR == NULL)
        pArcRec->filenameMOR = strdup("<unknown>");
    else
        pArcRec->filenameMOR = strdup(pRecord->filenameMOR);

    pArcRec->recordIdx = pRecord->recordIdx;
    pArcRec->numThreads = NuRecordGetNumThreads(pRecord);
    (void) NuRecordCopyThreads(pRecord, &pArcRec->pThreads);

    pArcRec->pNext = NULL;

    return pArcRec;
}

/*
 * Free up an ArchiveRecord.
 */
void ArchiveRecord_Free(ArchiveRecord* pArcRec)
{
    if (pArcRec == NULL)
        return;

    if (pArcRec->filenameMOR != NULL)
        free(pArcRec->filenameMOR);
    if (pArcRec->pThreads != NULL)
        free(pArcRec->pThreads);
    free(pArcRec);
}

/*
 * Find a thread with a matching NuThreadID.
 */
const NuThread* ArchiveRecord_FindThreadByID(const ArchiveRecord* pArcRec,
    NuThreadID threadID)
{
    const NuThread* pThread;
    int i;

    for (i = 0; i < pArcRec->numThreads; i++) {
        pThread = NuThreadGetByIdx(pArcRec->pThreads, i);
        if (NuGetThreadID(pThread) == threadID)
            return pThread;
    }

    return NULL;
}


const char* ArchiveRecord_GetFilename(const ArchiveRecord* pArcRec)
{
    return pArcRec->filenameMOR;
}

NuRecordIdx ArchiveRecord_GetRecordIdx(const ArchiveRecord* pArcRec)
{
    return pArcRec->recordIdx;
}

long ArchiveRecord_GetNumThreads(const ArchiveRecord* pArcRec)
{
    return pArcRec->numThreads;
}

const NuThread* ArchiveRecord_GetThread(const ArchiveRecord* pArcRec, int idx)
{
    if (idx < 0 || idx >= pArcRec->numThreads)
        return NULL;
    return NuThreadGetByIdx(pArcRec->pThreads, idx);
}

void ArchiveRecord_SetNext(ArchiveRecord* pArcRec, ArchiveRecord* pNextRec)
{
    pArcRec->pNext = pNextRec;
}

ArchiveRecord* ArchiveRecord_GetNext(const ArchiveRecord* pArcRec)
{
    return pArcRec->pNext;
}


/*
 * ===========================================================================
 *      ArchiveData
 * ===========================================================================
 */

/*
 * A collection of records.
 */
typedef struct ArchiveData {
    long            numRecords;
    ArchiveRecord*  pRecordHead;
    ArchiveRecord*  pRecordTail;
} ArchiveData;


ArchiveData* ArchiveData_New(void)
{
    ArchiveData* pArcData;

    pArcData = malloc(sizeof(*pArcData));
    if (pArcData == NULL)
        return NULL;

    pArcData->numRecords = 0;
    pArcData->pRecordHead = pArcData->pRecordTail = NULL;

    return pArcData;
}

void ArchiveData_Free(ArchiveData* pArcData)
{
    ArchiveRecord* pNext;

    if (pArcData == NULL)
        return;

    printf("*** Deleting %ld records!\n", pArcData->numRecords);
    while (pArcData->pRecordHead != NULL) {
        pNext = ArchiveRecord_GetNext(pArcData->pRecordHead);
        ArchiveRecord_Free(pArcData->pRecordHead);
        pArcData->pRecordHead = pNext;
    }

    free(pArcData);
}


ArchiveRecord* ArchiveData_GetRecordHead(const ArchiveData* pArcData)
{
    return pArcData->pRecordHead;
}


/* add an ArchiveRecord to the list pointed at by ArchiveData */
void ArchiveData_AddRecord(ArchiveData* pArcData, ArchiveRecord* pRecord)
{
    assert(pRecord != NULL);
    assert((pArcData->pRecordHead == NULL && pArcData->pRecordTail == NULL) ||
           (pArcData->pRecordHead != NULL && pArcData->pRecordTail != NULL));

    if (pArcData->pRecordHead == NULL) {
        /* first */
        pArcData->pRecordHead = pArcData->pRecordTail = pRecord;
    } else {
        /* not first, add to end */
        ArchiveRecord_SetNext(pArcData->pRecordTail, pRecord);
        pArcData->pRecordTail = pRecord;
    }

    pArcData->numRecords++;
}

/* dump the contents of the ArchiveData to stdout */
void ArchiveData_DumpContents(const ArchiveData* pArcData)
{
    ArchiveRecord* pArcRec;

    pArcRec = pArcData->pRecordHead;
    while (pArcRec != NULL) {
        const NuThread* pThread;
        int i, count;

        printf("%5u '%s'\n",
            ArchiveRecord_GetRecordIdx(pArcRec),
            ArchiveRecord_GetFilename(pArcRec));

        count = ArchiveRecord_GetNumThreads(pArcRec);
        for (i = 0; i < count; i++) {
            pThread = ArchiveRecord_GetThread(pArcRec, i);
            printf("    %5u 0x%04x 0x%04x\n", pThread->threadIdx,
                pThread->thThreadClass, pThread->thThreadKind);
        }

        pArcRec = ArchiveRecord_GetNext(pArcRec);
    }
}


/*
 * ===========================================================================
 *      Main stuff
 * ===========================================================================
 */

/*
 * Callback function to collect archive information.
 */
NuResult GatherContents(NuArchive* pArchive, void* vpRecord)
{
    NuRecord* pRecord = (NuRecord*) vpRecord;
    ArchiveData* pArchiveData = NULL;
    ArchiveRecord* pArchiveRecord = ArchiveRecord_New(pRecord);

    NuGetExtraData(pArchive, (void**)&pArchiveData);
    assert(pArchiveData != NULL);

    printf("*** Filename = '%s'\n",
        pRecord->filenameMOR == NULL ?
            "<unknown>" : pRecord->filenameMOR);

    ArchiveData_AddRecord(pArchiveData, pArchiveRecord);

    return kNuOK;
}


/*
 * Copy the filename thread from every record to "pDataSink".
 */
NuError ReadAllFilenameThreads(NuArchive* pArchive, ArchiveData* pArchiveData,
    NuDataSink* pDataSink)
{
    NuError err = kNuErrNone;
    ArchiveRecord* pArchiveRecord;
    const NuThread* pThread;

    pArchiveRecord = ArchiveData_GetRecordHead(pArchiveData);
    while (pArchiveRecord != NULL) {
        pThread = ArchiveRecord_FindThreadByID(pArchiveRecord,
                    kNuThreadIDFilename);
        if (pThread != NULL) {
            err = NuExtractThread(pArchive, pThread->threadIdx, pDataSink);
            if (err != kNuErrNone) {
                fprintf(stderr, "*** Extract failed (%d)\n", err);
                goto bail;
            }
        }
        pArchiveRecord = ArchiveRecord_GetNext(pArchiveRecord);
    }

bail:
    return err;
}


/* extract every filename thread into a single file, overwriting each time */
NuError ExtractToFile(NuArchive* pArchive, ArchiveData* pArchiveData)
{
    NuError err;
    NuDataSink* pDataSink = NULL;

    err = NuCreateDataSinkForFile(true, kNuConvertOff, "out.file", PATH_SEP,
            &pDataSink);
    if (err != kNuErrNone)
        goto bail;

    err = NuSetValue(pArchive, kNuValueHandleExisting, kNuAlwaysOverwrite);
    if (err != kNuErrNone)
        goto bail;

    err = ReadAllFilenameThreads(pArchive, pArchiveData, pDataSink);
    if (err != kNuErrNone)
        goto bail;

bail:
    (void) NuFreeDataSink(pDataSink);
    if (err == kNuErrNone)
        printf("*** File write complete\n");
    return err;
}

/* extract every filename thread into a FILE*, appending */
NuError ExtractToFP(NuArchive* pArchive, ArchiveData* pArchiveData)
{
    NuError err;
    FILE* fp = NULL;
    NuDataSink* pDataSink = NULL;

    if ((fp = fopen("out.fp", kNuFileOpenWriteTrunc)) == NULL)
        return kNuErrFileOpen;

    err = NuCreateDataSinkForFP(true, kNuConvertOff, fp, &pDataSink);
    if (err != kNuErrNone)
        goto bail;

    err = ReadAllFilenameThreads(pArchive, pArchiveData, pDataSink);
    if (err != kNuErrNone)
        goto bail;

bail:
    (void) NuFreeDataSink(pDataSink);
    if (fp != NULL)
        fclose(fp);
    if (err == kNuErrNone)
        printf("*** FP write complete\n");
    return err;
}

/* extract every filename thread into a buffer, advancing as we go */
NuError ExtractToBuffer(NuArchive* pArchive, ArchiveData* pArchiveData)
{
    NuError err;
    uint8_t buffer[kHappySize];
    NuDataSink* pDataSink = NULL;
    uint32_t count;

    err = NuCreateDataSinkForBuffer(true, kNuConvertOff, buffer, kHappySize,
            &pDataSink);
    if (err != kNuErrNone)
        goto bail;

    err = ReadAllFilenameThreads(pArchive, pArchiveData, pDataSink);
    if (err != kNuErrNone) {
        if (err == kNuErrBufferOverrun)
            fprintf(stderr, "*** Hey, buffer wasn't big enough!\n");
        goto bail;
    }

    /* write the buffer to a file */
    (void) NuDataSinkGetOutCount(pDataSink, &count);
    if (count > 0) {
        FILE* fp;
        if ((fp = fopen("out.buf", kNuFileOpenWriteTrunc)) != NULL) {

            printf("*** Writing %u bytes\n", count);
            if (fwrite(buffer, count, 1, fp) != 1)
                err = kNuErrFileWrite;
            fclose(fp);
        }
    } else {
        printf("*** No data found!\n");
    }

bail:
    (void) NuFreeDataSink(pDataSink);
    return err;
}


/*
 * Do file stuff.
 */
int DoFileStuff(const UNICHAR* filenameUNI)
{
    NuError err;
    NuArchive* pArchive = NULL;
    ArchiveData* pArchiveData = ArchiveData_New();

    err = NuOpenRO(filenameUNI, &pArchive);
    if (err != kNuErrNone)
        goto bail;

    NuSetExtraData(pArchive, pArchiveData);

    printf("*** Gathering contents!\n");
    err = NuContents(pArchive, GatherContents);
    if (err != kNuErrNone)
        goto bail;

    printf("*** Dumping contents!\n");
    ArchiveData_DumpContents(pArchiveData);

    err = ExtractToFile(pArchive, pArchiveData);
    if (err != kNuErrNone)
        goto bail;
    err = ExtractToFP(pArchive, pArchiveData);
    if (err != kNuErrNone)
        goto bail;
    err = ExtractToBuffer(pArchive, pArchiveData);
    if (err != kNuErrNone)
        goto bail;

bail:
    if (err != kNuErrNone)
        fprintf(stderr, "*** ERROR: got error %d\n", err);

    if (pArchive != NULL) {
        NuError err2 = NuClose(pArchive);
        if (err == kNuErrNone && err2 != kNuErrNone)
            err = err2;
    }

    ArchiveData_Free(pArchiveData);

    return err;
}


/*
 * Grab the name of an archive to read.  If no name was provided, use stdin.
 */
int main(int argc, char** argv)
{
    int32_t major, minor, bug;
    const char* pBuildDate;
    FILE* infp = NULL;
    int cc;

    (void) NuGetVersion(&major, &minor, &bug, &pBuildDate, NULL);
    printf("Using NuFX lib %d.%d.%d built on or after %s\n",
        major, minor, bug, pBuildDate);

    if (argc == 2) {
        infp = fopen(argv[1], kNuFileOpenReadOnly);
        if (infp == NULL) {
            perror("fopen failed");
            exit(1);
        }
    } else {
        fprintf(stderr, "ERROR: you have to specify a filename\n");
        exit(2);
    }

    cc = DoFileStuff(argv[1]);

    if (infp != NULL)
        fclose(infp);

    exit(cc != 0);
}

