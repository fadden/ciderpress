/*
 * NuFX archive manipulation library
 * Copyright (C) 2000-2007 by Andy McFadden, All Rights Reserved.
 * This is free software; you can redistribute it and/or modify it under the
 * terms of the BSD License, see the file COPYING.LIB.
 *
 * Recompress records in place, several times, possibly deleting records
 * or threads as we go.  The goal is to perform a large number of operations
 * that modify the archive without closing and reopening it.
 *
 * Depending on which #defines are enabled, this can be very destructive,
 * so a copy of the archive is made before processing begins.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "NufxLib.h"
#include "Common.h"

/* copy the archive to this file before starting */
static const char* kWorkFileName = "TwirlCopy678";
static const char* kTempFileName = "TwirlTmp789";

/* after loading this much stuff into memory, flush changes */
const int kMaxHeldLen = 1024 * 1024;

/*
 * A list of CRCs.
 */
typedef struct CRCList {
    int         numEntries;
    uint16_t*   entries;
} CRCList;


/*
 * Returns true if the compression type is supported, false otherwise.
 */
int CompressionSupported(NuValue compression)
{
    int result;

    switch (compression) {
    case kNuCompressNone:
        result = true;
        break;
    case kNuCompressSQ:
        result = (NuTestFeature(kNuFeatureCompressSQ) == kNuErrNone);
        break;
    case kNuCompressLZW1:
    case kNuCompressLZW2:
        result = (NuTestFeature(kNuFeatureCompressLZW) == kNuErrNone);
        break;
    case kNuCompressLZC12:
    case kNuCompressLZC16:
        result = (NuTestFeature(kNuFeatureCompressLZC) == kNuErrNone);
        break;
    case kNuCompressDeflate:
        result = (NuTestFeature(kNuFeatureCompressDeflate) == kNuErrNone);
        break;
    case kNuCompressBzip2:
        result = (NuTestFeature(kNuFeatureCompressBzip2) == kNuErrNone);
        break;
    default:
        assert(false);
        result = false;
    }

    /*printf("Returning %d for %ld\n", result, compression);*/

    return result;
}

/* 
 * This gets called when a buffer DataSource is no longer needed.
 */
NuResult FreeCallback(NuArchive* pArchive, void* args)
{
    free(args);
    return kNuOK; 
}


/*
 * Dump a CRC list.
 */
void DumpCRCs(const CRCList* pCRCList)
{
    int i;

    printf(" NumEntries: %d\n", pCRCList->numEntries);

    for (i = 0; i < pCRCList->numEntries; i++)
        printf("   %5d: 0x%04x\n", i, pCRCList->entries[i]);
}

/*
 * Free a CRC list.
 */
void FreeCRCs(CRCList* pCRCList)
{
    if (pCRCList == NULL)
        return;

    free(pCRCList->entries);
    free(pCRCList);
}

/*
 * Gather a list of CRCs from the archive.
 *
 * We assume there are at most two data threads (e.g. data fork and rsrc
 * fork) in a record.
 *
 * Returns the list on success, NULL on failure.
 */
CRCList* GatherCRCs(NuArchive* pArchive)
{
    NuError err = kNuErrNone;
    const NuMasterHeader* pMasterHeader;
    CRCList* pCRCList = NULL;
    uint16_t* pEntries = NULL;
    long recCount, maxCRCs;
    long recIdx, crcIdx;
    int i;

    pCRCList = malloc(sizeof(*pCRCList));
    if (pCRCList == NULL) {
        fprintf(stderr, "ERROR: couldn't alloc CRC list\n");
        err = kNuErrGeneric;
        goto bail;
    }
    memset(pCRCList, 0, sizeof(*pCRCList));

    /* get record count out of master header, just for fun */
    err = NuGetMasterHeader(pArchive, &pMasterHeader);
    if (err != kNuErrNone) {
        fprintf(stderr, "ERROR: couldn't get master header (err=%d)\n", err);
        goto bail;
    }
    recCount = pMasterHeader->mhTotalRecords;
    maxCRCs = recCount * 2;

    pEntries = malloc(sizeof(*pEntries) * maxCRCs);
    if (pEntries == NULL) {
        fprintf(stderr, "ERROR: unable to alloc CRC list (%ld entries)\n",
            maxCRCs);
        err = kNuErrGeneric;
        goto bail;
    }
    pCRCList->entries = pEntries;

    for (i = 0; i < maxCRCs; i++)
        pEntries[i] = 0xdead;

    /*
     * Enumerate our way through the records.  If something was disturbed
     * we should end up in a different place and the CRCs will be off.
     */
    crcIdx = 0;
    for (recIdx = 0; recIdx < recCount; recIdx++) {
        NuRecordIdx recordIdx;
        const NuRecord* pRecord;
        const NuThread* pThread;

        err = NuGetRecordIdxByPosition(pArchive, recIdx, &recordIdx);
        if (err != kNuErrNone) {
            fprintf(stderr, "ERROR: couldn't get record #%ld (err=%d)\n",
                recIdx, err);
            goto bail;
        }

        err = NuGetRecord(pArchive, recordIdx, &pRecord);
        if (err != kNuErrNone) {
            fprintf(stderr, "ERROR: unable to get recordIdx %u\n", recordIdx);
            goto bail;
        }

        if (NuRecordGetNumThreads(pRecord) == 0) {
            fprintf(stderr, "ERROR: not expecting empty record (%u)!\n",
                recordIdx);
            err = kNuErrGeneric;
            goto bail;
        }

        int rsrcCrcIdx = -1;
        for (i = 0; i < (int)NuRecordGetNumThreads(pRecord); i++) {
            pThread = NuGetThread(pRecord, i);
            if (pThread->thThreadClass == kNuThreadClassData) {
                if (crcIdx >= maxCRCs) {
                    fprintf(stderr, "ERROR: CRC buffer exceeded\n");
                    assert(false);
                    err = kNuErrGeneric;
                    goto bail;
                }

                /*
                 * Ensure that the data fork CRC comes first.  Otherwise
                 * we can fail if it gets rearranged.  This is only a
                 * problem for GSHK-created archives that don't have
                 * threads for every fork, so "mask dataless" is create
                 * fake entries.
                 *
                 * The correct way to do this is to store a tuple
                 * { thread-kind, crc }, but that's more work.
                 */
                if (pThread->thThreadKind == kNuThreadKindRsrcFork) {
                    rsrcCrcIdx = crcIdx;
                }

                if (pThread->thThreadKind == kNuThreadKindDataFork &&
                    rsrcCrcIdx != -1)
                {
                    /* this is the data fork, we've already seen the
                       resource fork; swap entries */
                    pEntries[crcIdx++] = pEntries[rsrcCrcIdx];
                    pEntries[rsrcCrcIdx] = pThread->thThreadCRC;
                } else {
                    pEntries[crcIdx++] = pThread->thThreadCRC;
                }
            }
        }
    }

    pCRCList->numEntries = crcIdx;

    DumpCRCs(pCRCList);

bail:
    if (err != kNuErrNone) {
        FreeCRCs(pCRCList);
        pCRCList = NULL;
    }
    return pCRCList;
}


/*
 * Compare the current set of CRCs against our saved list.  If any of
 * the records or threads were deleted or rearranged, this will fail.
 * I happen to think this is a *good* thing: if something is the least
 * bit screwy, I want to know about it.
 *
 * Unfortunately, if we *deliberately* delete records, this can't
 * help us with the survivors.
 *
 * Returns 0 on success, nonzero on failure.
 */
int CompareCRCs(NuArchive* pArchive, const CRCList* pOldCRCList)
{
    CRCList* pNewCRCList = NULL;
    int result = -1;
    int badCrc = 0;
    int i;

    pNewCRCList = GatherCRCs(pArchive);
    if (pNewCRCList == NULL) {
        fprintf(stderr, "ERROR: unable to gather new list\n");
        goto bail;
    }

    if (pOldCRCList->numEntries != pNewCRCList->numEntries) {
        fprintf(stderr, "ERROR: numEntries mismatch: %d vs %d\n",
            pOldCRCList->numEntries, pNewCRCList->numEntries);
        goto bail;
    }

    for (i = 0; i < pNewCRCList->numEntries; i++) {
        if (pOldCRCList->entries[i] != pNewCRCList->entries[i]) {
            fprintf(stderr, "ERROR: CRC mismatch: %5d old=0x%04x new=0x%04x\n",
                i, pOldCRCList->entries[i], pNewCRCList->entries[i]);
            badCrc = 1;
        }
    }
    if (!badCrc) {
        printf("  Matched %d CRCs\n", pOldCRCList->numEntries);
        result = 0;
    }

bail:
    FreeCRCs(pNewCRCList);
    return result;
}


/*
 * Recompress a single thread.
 *
 * This entails (1) extracting the existing thread, (2) deleting the
 * thread, and (3) adding the extracted data.
 *
 * All of this good stuff gets queued up until the next NuFlush call.
 */
NuError RecompressThread(NuArchive* pArchive, const NuRecord* pRecord,
    const NuThread* pThread)
{
    NuError err = kNuErrNone;
    NuDataSource* pDataSource = NULL;
    NuDataSink* pDataSink = NULL;
    uint8_t* buf = NULL;

    if (pThread->actualThreadEOF == 0) {
        buf = malloc(1);
        if (buf == NULL) {
            fprintf(stderr, "ERROR: failed allocating trivial buffer\n");
            err = kNuErrGeneric;
            goto bail;
        }
    } else {
        /*
         * Create a buffer and data sink to hold the data.
         */
        buf = malloc(pThread->actualThreadEOF);
        if (buf == NULL) {
            fprintf(stderr, "ERROR: failed allocating %u bytes\n",
                pThread->actualThreadEOF);
            err = kNuErrGeneric;
            goto bail;
        }

        err = NuCreateDataSinkForBuffer(true, kNuConvertOff, buf,
                pThread->actualThreadEOF, &pDataSink);
        if (err != kNuErrNone) {
            fprintf(stderr, "ERROR: unable to create data sink (err=%d)\n",err);
            goto bail;
        }

        /*
         * Extract the data.
         */
        err = NuExtractThread(pArchive, pThread->threadIdx, pDataSink);
        if (err != kNuErrNone) {
            fprintf(stderr, "ERROR: failed extracting thread %u in '%s': %s\n",
                pThread->threadIdx, pRecord->filenameMOR, NuStrError(err));
            goto bail;
        }
    }

    /*
     * Delete the existing thread.
     */
    err = NuDeleteThread(pArchive, pThread->threadIdx);
    if (err != kNuErrNone) {
        fprintf(stderr, "ERROR: unable to delete thread %u\n",
            pThread->threadIdx);
        goto bail;
    }

    /*
     * Create a data source for the new thread.  Specify a callback to free
     * the buffer when NufxLib is done with it.
     */
    err = NuCreateDataSourceForBuffer(kNuThreadFormatUncompressed,
            0, buf, 0, pThread->actualThreadEOF, FreeCallback, &pDataSource);
    if (err != kNuErrNone) {
        fprintf(stderr, "ERROR: unable to create data source (err=%d)\n", err);
        goto bail;
    }
    buf = NULL;

    /*
     * Create replacement thread.
     */
    err = NuAddThread(pArchive, pRecord->recordIdx, NuGetThreadID(pThread),
            pDataSource, NULL);
    if (err != kNuErrNone) {
        fprintf(stderr, "ERROR: unable to add new thread ID=0x%08x (err=%d)\n",
            NuGetThreadID(pThread), err);
        goto bail;
    }
    pDataSource = NULL;      /* now owned by NufxLib */

bail:
    NuFreeDataSink(pDataSink);
    NuFreeDataSource(pDataSource);
    free(buf);
    return err;
}

/*
 * Recompress a single record.
 *
 * The amount of data we're holding in memory as a result of the
 * recompression is placed in "*pLen".
 */
NuError RecompressRecord(NuArchive* pArchive, NuRecordIdx recordIdx, long* pLen)
{
    NuError err = kNuErrNone;
    const NuRecord* pRecord;
    const NuThread* pThread;
    int i;

    printf("  Recompressing %u\n", recordIdx);

    *pLen = 0;

    err = NuGetRecord(pArchive, recordIdx, &pRecord);
    if (err != kNuErrNone) {
        fprintf(stderr, "ERROR: unable to get record %u (err=%d)\n",
            recordIdx, err);
        goto bail;
    }

    for (i = 0; i < (int)NuRecordGetNumThreads(pRecord); i++) {
        pThread = NuGetThread(pRecord, i);
        if (pThread->thThreadClass == kNuThreadClassData) {
            /*printf("    Recompressing %d (threadID=0x%08lx)\n", i,
                NuGetThreadID(pThread));*/
            err = RecompressThread(pArchive, pRecord, pThread);
            if (err != kNuErrNone) {
                fprintf(stderr, "ERROR: failed recompressing thread %u "
                                " in record %u (err=%d)\n",
                    pThread->threadIdx, pRecord->recordIdx, err);
                goto bail;
            }
            *pLen += pThread->actualThreadEOF;
        } else {
            /*printf("    Skipping %d (threadID=0x%08lx)\n", i,
                NuGetThreadID(pThread));*/
        }
    }

bail:
    return err;
}

/*
 * Recompress every data thread in the archive.
 */
NuError RecompressArchive(NuArchive* pArchive, NuValue compression)
{
    NuError err = kNuErrNone;
    NuRecordIdx* pIndices = NULL;
    NuAttr countAttr;
    long heldLen;
    long idx;

    err = NuSetValue(pArchive, kNuValueDataCompression, compression);
    if (err != kNuErrNone) {
        fprintf(stderr, "ERROR: unable to set compression to %u (err=%d)\n",
            compression, err);
        goto bail;
    }

    printf("Recompressing threads with compression type %u\n", compression);

    err = NuGetAttr(pArchive, kNuAttrNumRecords, &countAttr);
    if (err != kNuErrNone) {
        fprintf(stderr, "ERROR: unable to get numRecords (err=%d)\n", err);
        goto bail;
    }

    if (countAttr == 0) {
        printf("No records found!\n");
        goto bail;
    }

    /*
     * Get all of the indices up front.  This way, if something causes a
     * record to "disappear" during processing, we will know about it.
     */
    pIndices = malloc(countAttr * sizeof(*pIndices));
    if (pIndices == NULL) {
        fprintf(stderr, "ERROR: malloc on %u indices failed\n", countAttr);
        err = kNuErrGeneric;
        goto bail;
    }

    for (idx = 0; idx < (int)countAttr; idx++) {
        err = NuGetRecordIdxByPosition(pArchive, idx, &pIndices[idx]);
        if (err != kNuErrNone) {
            fprintf(stderr, "ERROR: couldn't get record #%ld (err=%d)\n",
                idx, err);
            goto bail;
        }
    }

    /*
     * Walk through the index list, handling each record individually.
     */
    heldLen = 0;
    for (idx = 0; idx < (int)countAttr; idx++) {
        long recHeldLen;

        err = RecompressRecord(pArchive, pIndices[idx], &recHeldLen);
        if (err != kNuErrNone) {
            fprintf(stderr, "ERROR: failed recompressing record %u (err=%d)\n",
                pIndices[idx], err);
            goto bail;
        }

        heldLen += recHeldLen;

        if (heldLen > kMaxHeldLen) {
            uint32_t statusFlags;

            printf("    (flush)\n");
            err = NuFlush(pArchive, &statusFlags);
            if (err != kNuErrNone) {
                fprintf(stderr, "ERROR: intra-recompress flush failed: %s\n",
                    NuStrError(err));
                goto bail;
            }

            heldLen = 0;
        }
    }

bail:
    free(pIndices);
    return err;
}

/*
 * Initiate the twirling.
 */
int TwirlArchive(const char* filename)
{
    NuError err = kNuErrNone;
    NuArchive* pArchive = NULL;
    CRCList* pCRCList = NULL;
    int compression;
    int cc;

    /*
     * Open the archive after removing any temp file remnants.
     */
    cc = unlink(kTempFileName);
    if (cc == 0)
        printf("Removed stale temp file '%s'\n", kTempFileName);

    err = NuOpenRW(filename, kTempFileName, 0, &pArchive);
    if (err != kNuErrNone) {
        fprintf(stderr, "ERROR: unable to open archive '%s': %s\n",
            filename, NuStrError(err));
        goto bail;
    }

    /*
     * Mask records with no data threads, so we don't have to
     * special-case them.
     */
    err = NuSetValue(pArchive, kNuValueMaskDataless, true);
    if (err != kNuErrNone) {
        fprintf(stderr, "ERROR: couldn't mask dataless (err=%d)\n", err);
        goto bail;
    }

    pCRCList = GatherCRCs(pArchive);
    if (pCRCList == NULL) {
        fprintf(stderr, "ERROR: unable to get CRC list\n");
        goto bail;
    }

    /*
     * For each type of compression, recompress the entire archive.
     */
    for (compression = kNuCompressNone; compression <= kNuCompressBzip2;
        compression++)
    {
        uint32_t statusFlags;

        if (!CompressionSupported(compression))
            continue;

        err = RecompressArchive(pArchive, compression);
        if (err != kNuErrNone) {
            fprintf(stderr, "ERROR: recompress failed: %s\n", NuStrError(err));
            goto bail;
        }

        err = NuFlush(pArchive, &statusFlags);
        if (err != kNuErrNone) {
            fprintf(stderr, "ERROR: post-recompress flush failed: %s\n",
                NuStrError(err));
            goto bail;
        }
    }

    /*
     * Same thing, reverse order.  We want to start with the same one we
     * ended on above, so we can practice skipping over things.
     */
    for (compression = kNuCompressBzip2; compression >= kNuCompressNone;
        compression--)
    {
        uint32_t statusFlags;

        if (!CompressionSupported(compression))
            continue;

        err = RecompressArchive(pArchive, compression);
        if (err != kNuErrNone) {
            fprintf(stderr, "ERROR: recompress2 failed: %s\n", NuStrError(err));
            goto bail;
        }

        err = NuFlush(pArchive, &statusFlags);
        if (err != kNuErrNone) {
            fprintf(stderr, "ERROR: post-recompress flush2 failed: %s\n",
                NuStrError(err));
            goto bail;
        }
    }

    if (CompareCRCs(pArchive, pCRCList) != 0) {
        fprintf(stderr, "ERROR: CRCs didn't match\n");
        goto bail;
    }

    printf("Done!\n");

bail:
    FreeCRCs(pCRCList);
    if (pArchive != NULL) {
        NuAbort(pArchive);
        NuClose(pArchive);
    }

    return (err != kNuErrNone);
}


/*
 * Copy from the current offset in "srcfp" to a new file called
 * "outFileName".  Returns a writable file descriptor for the new file
 * on success, or NULL on error.
 *
 * (Note "CopyFile()" exists under Win32.)
 */
FILE* MyCopyFile(const char* outFileName, FILE* srcfp)
{
    char buf[24576];
    FILE* outfp;
    size_t count;

    outfp = fopen(outFileName, kNuFileOpenWriteTrunc);
    if (outfp == NULL) {
        fprintf(stderr, "ERROR: unable to open '%s' (err=%d)\n", outFileName,
            errno);
        return NULL;
    }

    while (!feof(srcfp)) {
        count = fread(buf, 1, sizeof(buf), srcfp);
        if (count == 0)
            break;
        if (fwrite(buf, 1, count, outfp) != count) {
            fprintf(stderr, "ERROR: failed writing outfp (err=%d)\n", errno);
            fclose(outfp);
            return NULL;
        }
    }

    if (ferror(srcfp)) {
        fprintf(stderr, "ERROR: failed reading srcfp (err=%d)\n", errno);
        fclose(outfp);
        return NULL;
    }

    return outfp;
}

/*
 * Let's get started.
 */
int main(int argc, char** argv)
{
    int32_t major, minor, bug;
    const char* pBuildDate;
    FILE* srcfp = NULL;
    FILE* infp = NULL;
    int cc;

    /* don't buffer output */
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);

    (void) NuGetVersion(&major, &minor, &bug, &pBuildDate, NULL);
    printf("Using NuFX lib %d.%d.%d built on or after %s\n\n",
        major, minor, bug, pBuildDate);

    if (argc == 2) {
        srcfp = fopen(argv[1], kNuFileOpenReadOnly);
        if (srcfp == NULL) {
            perror("fopen failed");
            exit(1);
        }
    } else {
        fprintf(stderr, "ERROR: you have to specify a filename\n");
        exit(2);
    }

    printf("Copying '%s' to '%s'\n", argv[1], kWorkFileName);

    infp = MyCopyFile(kWorkFileName, srcfp);
    if (infp == NULL) {
        fprintf(stderr, "Copy failed, bailing.\n");
        exit(1);
    }
    fclose(srcfp);
    fclose(infp);

    cc = TwirlArchive(kWorkFileName);

    exit(cc != 0);
}

