/*
 * NuFX archive manipulation library
 * Copyright (C) 2000-2007 by Andy McFadden, All Rights Reserved.
 * This is free software; you can redistribute it and/or modify it under the
 * terms of the BSD License, see the file COPYING.LIB.
 *
 * Run an archive through the laundry.  The net result is a duplicate
 * archive that matches the original in most respects.  Extracting the
 * files from the duplicate will yield the same results as if they were
 * extracted from the original, but the duplicate archive may differ
 * in subtle ways (e.g. filename threads may be added, data may be
 * recompressed).
 *
 * This demonstrates copying threads around, both with and without
 * recompressing, between two archives that are open simultaneously.  This
 * also tests NufxLib's thread ordering and verifies that you can abort
 * frequently with no adverse effects.
 *
 * NOTE: depending on the options you select, you may need to have enough
 * memory to hold the entire uncompressed contents of the original archive.
 * The memory requirements are reduced if you use the "copy only" flag, and
 * are virtually eliminated if you use "frequent flush".
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "NufxLib.h"
#include "Common.h"


#define kTempFile   "tmp-laundry"

#define kFlagCopyOnly           (1)
#define kFlagReverseThreads     (1 << 1)
#define kFlagFrequentFlush      (1 << 2)
#define kFlagFrequentAbort      (1 << 3)    /* implies FrequentFlush */
#define kFlagUseTmp             (1 << 4)


/*
 * Globals.
 */
char gSentRecordWarning = false;


/*
 * This gets called when a buffer DataSource is no longer needed.
 */
NuResult
FreeCallback(NuArchive* pArchive, void* args)
{
    free(args);
    return kNuOK;
}

/*
 * Copy a thread, expanding and recompressing it.
 *
 * This assumes the library is configured for compression (it defaults
 * to LZW/2, so this is a reasonable assumption).
 */
NuError
CopyThreadRecompressed(NuArchive* pInArchive, NuArchive* pOutArchive,
    long flags, const NuThread* pThread, long newRecordIdx)
{
    NuError err = kNuErrNone;
    NuDataSource* pDataSource = nil;
    NuDataSink* pDataSink = nil;
    uchar* buffer = nil;

    /*
     * Allocate a buffer large enough to hold all the uncompressed data, and
     * wrap a data sink around it.
     *
     * If the thread is zero bytes long, we can skip this part.
     */
    if (pThread->actualThreadEOF) {
        buffer = malloc(pThread->actualThreadEOF);
        if (buffer == nil) {
            err = kNuErrMalloc;
            goto bail;
        }
        err = NuCreateDataSinkForBuffer(true, kNuConvertOff, buffer,
                pThread->actualThreadEOF, &pDataSink);
        if (err != kNuErrNone) {
            fprintf(stderr, "ERROR: unable to create data sink (err=%d)\n",
                err);
            goto bail;
        }

        /*
         * Expand the data.  For a pre-sized thread, this grabs only the
         * interesting part of the buffer.
         */
        err = NuExtractThread(pInArchive, pThread->threadIdx, pDataSink);
        if (err != kNuErrNone) {
            fprintf(stderr, "ERROR: unable to extract thread %ld (err=%d)\n",
                pThread->threadIdx, err);
            goto bail;
        }
    }

    /*
     * The expanded data is in the buffer, now create a data source that
     * describes it.
     *
     * This is complicated by the existence of pre-sized threads, which
     * require us to set "otherLen".
     *
     * We always use "actualThreadEOF" because "thThreadEOF" is broken
     * for disk archives created by certain versions of ShrinkIt.
     *
     * It's okay to pass in a nil value for "buffer", so long as the
     * amount of data in the buffer is also zero.  The library will do
     * the right thing.
     */
    if (NuIsPresizedThreadID(NuGetThreadID(pThread))) {
        err = NuCreateDataSourceForBuffer(kNuThreadFormatUncompressed,
                pThread->thCompThreadEOF, buffer, 0,
                pThread->actualThreadEOF, FreeCallback, &pDataSource);
        if (err != kNuErrNone) {
            fprintf(stderr,
                "ERROR: unable to create pre-sized data source (err=%d)\n",err);
            goto bail;
        }
    } else {
        err = NuCreateDataSourceForBuffer(kNuThreadFormatUncompressed,
                0, buffer, 0, pThread->actualThreadEOF,
                FreeCallback, &pDataSource);
        if (err != kNuErrNone) {
            fprintf(stderr,
                "ERROR: unable to create data source (err=%d)\n", err);
            goto bail;
        }
    }
    buffer = nil;   /* doClose was set, so it's owned by the data source */

    /*
     * Schedule the data for addition to the record.
     */
    err = NuAddThread(pOutArchive, newRecordIdx, NuGetThreadID(pThread),
            pDataSource, nil);
    if (err != kNuErrNone) {
        fprintf(stderr, "ERROR: unable to add thread (err=%d)\n", err);
        goto bail;
    }
    pDataSource = nil;  /* library owns it now */

bail:
    if (pDataSource != nil)
        NuFreeDataSource(pDataSource);
    if (pDataSink != nil)
        NuFreeDataSink(pDataSink);
    if (buffer != nil)
        free(buffer);
    return err;
}

/*
 * Copy a thread from one archive to another without disturbing the
 * compression.
 *
 * There is a much more efficient way to do this: create an FP
 * data source using an offset within the archive file itself.
 * Since pInArchive->archiveFp isn't exposed, we can't use that,
 * but under most operating systems you aren't prevented from
 * opening the same file twice in read-only mode.  The file offset
 * in pThread tells us where the data is.
 *
 * The method used below is less memory-efficient but more portable.
 *
 * This always extracts based on the compThreadEOF, which is
 * reliable but extracts a little more than we need on pre-sized
 * threads (filenames, comments).
 */
NuError
CopyThreadUncompressed(NuArchive* pInArchive, NuArchive* pOutArchive,
    long flags, const NuThread* pThread, long newRecordIdx)
{
    NuError err = kNuErrNone;
    NuDataSource* pDataSource = nil;
    NuDataSink* pDataSink = nil;
    uchar* buffer = nil;

    /*
     * If we have some data files that were left uncompressed, perhaps
     * because of GSHK's "don't compress anything smaller than 512 bytes"
     * rule, NufxLib will try to compress them.  We disable this
     * behavior by disabling compression.  That way, stuff that is
     * already compressed will remain that way, and stuff that isn't
     * compressed won't be.  (We really only need to do this once, at
     * the start of the program, but it's illustrative to do it here.)
     *
     * [ I don't understand this comment.  It's necessary to disable
     *   compression, but I don't see why uncompressed files are
     *   special. ++ATM 20040821 ]
     */
    err = NuSetValue(pOutArchive, kNuValueDataCompression, kNuCompressNone);
    if (err != kNuErrNone) {
        fprintf(stderr, "ERROR: unable to set compression (err=%d)\n", err);
        goto bail;
    }

    /*
     * Allocate a buffer large enough to hold all the compressed data, and
     * wrap a data sink around it.
     */
    buffer = malloc(pThread->thCompThreadEOF);
    if (buffer == nil) {
        err = kNuErrMalloc;
        goto bail;
    }
    err = NuCreateDataSinkForBuffer(false, kNuConvertOff, buffer,
            pThread->thCompThreadEOF, &pDataSink);
    if (err != kNuErrNone) {
        fprintf(stderr, "ERROR: unable to create data sink (err=%d)\n",
            err);
        goto bail;
    }

    /*
     * Get the compressed data.  For a pre-sized thread, this grabs the
     * entire contents of the buffer, including the padding.
     */
    err = NuExtractThread(pInArchive, pThread->threadIdx, pDataSink);
    if (err != kNuErrNone) {
        fprintf(stderr, "ERROR: unable to extract thread %ld (err=%d)\n",
            pThread->threadIdx, err);
        goto bail;
    }

    /*
     * The (perhaps compressed) data is in the buffer, now create a data
     * source that describes it.
     *
     * This is complicated by the existence of pre-sized threads.  There
     * are two possibilities:
     *  1. We have a certain amount of non-pre-sized data (thCompThreadEOF)
     *     that will expand out to a certain length (actualThreadEOF).
     *  2. We have a certain amount of pre-sized data (actualThreadEOF)
     *     that will fit within a buffer (thCompThreadEOF).
     * As you can see, the arguments need to be reversed for pre-sized
     * threads.
     *
     * We always use "actualThreadEOF" because "thThreadEOF" is broken
     * for disk archives created by certain versions of ShrinkIt.
     */
    if (NuIsPresizedThreadID(NuGetThreadID(pThread))) {
        err = NuCreateDataSourceForBuffer(pThread->thThreadFormat,
                pThread->thCompThreadEOF, buffer, 0,
                pThread->actualThreadEOF, FreeCallback, &pDataSource);
        if (err != kNuErrNone) {
            fprintf(stderr,
                "ERROR: unable to create pre-sized data source (err=%d)\n",err);
            goto bail;
        }
    } else {
        err = NuCreateDataSourceForBuffer(pThread->thThreadFormat,
                pThread->actualThreadEOF, buffer, 0,
                pThread->thCompThreadEOF, FreeCallback, &pDataSource);
        if (err != kNuErrNone) {
            fprintf(stderr,
                "ERROR: unable to create data source (err=%d)\n", err);
            goto bail;
        }
    }
    buffer = nil;   /* doClose was set, so it's owned by the data source */

    /* yes, this is a kluge... sigh */
    err = NuDataSourceSetRawCrc(pDataSource, pThread->thThreadCRC);
    if (err != kNuErrNone) {
        fprintf(stderr, "ERROR: can't set source CRC (err=%d)\n", err);
        goto bail;
    }

    /*
     * Schedule the data for addition to the record.
     *
     * Note that NuAddThread makes a copy of the data source, and clears
     * "doClose" on our copy, so we are free to dispose of pDataSource.
     */
    err = NuAddThread(pOutArchive, newRecordIdx, NuGetThreadID(pThread),
            pDataSource, nil);
    if (err != kNuErrNone) {
        fprintf(stderr, "ERROR: unable to add thread (err=%d)\n", err);
        goto bail;
    }
    pDataSource = nil;  /* library owns it now */

bail:
    if (pDataSource != nil)
        NuFreeDataSource(pDataSource);
    if (pDataSink != nil)
        NuFreeDataSink(pDataSink);
    if (buffer != nil)
        free(buffer);
    return err;
}


/*
 * Copy a thread from one archive to another.
 *
 * Depending on "flags", this will either copy it raw or uncompress and
 * recompress.
 */
NuError
CopyThread(NuArchive* pInArchive, NuArchive* pOutArchive, long flags,
    const NuThread* pThread, long newRecordIdx)
{
    if (flags & kFlagCopyOnly) {
        return CopyThreadUncompressed(pInArchive, pOutArchive, flags, pThread,
                newRecordIdx);
    } else {
        return CopyThreadRecompressed(pInArchive, pOutArchive, flags, pThread,
                newRecordIdx);
    }
}


/*
 * Copy a record from the input to the output.
 *
 * This runs through the list of threads and copies each one individually.
 * It will copy them in the original order or in reverse order (the latter
 * of which will not usually have any effect since NufxLib imposes a
 * specific thread ordering on most common types) depending on "flags".
 */
NuError
CopyRecord(NuArchive* pInArchive, NuArchive* pOutArchive, long flags,
    NuRecordIdx recordIdx)
{
    NuError err = kNuErrNone;
    const NuRecord* pRecord;
    const NuThread* pThread;
    NuFileDetails fileDetails;
    NuRecordIdx newRecordIdx;
    long numThreads;
    int idx;

    /*
     * Grab the original record and see how many threads it has.
     */
    err = NuGetRecord(pInArchive, recordIdx, &pRecord);
    if (err != kNuErrNone) {
        fprintf(stderr, "ERROR: unable to get recordIdx %ld\n", recordIdx);
        goto bail;
    }

    /*
     * Pre-v3 records didn't put CRCs in the thread headers.  If we just
     * copy the thread over without reprocessing the data, we won't compute
     * a CRC for the thread, and we will get CRC failures.
     */
    if (!gSentRecordWarning && (flags & kFlagCopyOnly) &&
        pRecord->recVersionNumber < 3)
    {
        printf("WARNING: pre-v3 records that aren't recompressed may exhibit CRC failures\n");
        gSentRecordWarning = true;
    }

    numThreads = NuRecordGetNumThreads(pRecord);
    if (!numThreads) {
        fprintf(stderr, "WARNING: recordIdx=%ld was empty\n", recordIdx);
        goto bail;
    }

    /*
     * Create a new record that looks just like the original.
     */
    memset(&fileDetails, 0, sizeof(fileDetails));
    fileDetails.storageName = pRecord->filename;
    fileDetails.fileSysID = pRecord->recFileSysID;
    fileDetails.fileSysInfo = pRecord->recFileSysInfo;
    fileDetails.access = pRecord->recAccess;
    fileDetails.fileType = pRecord->recFileType;
    fileDetails.extraType = pRecord->recExtraType;
    fileDetails.storageType = pRecord->recStorageType;
    fileDetails.createWhen = pRecord->recCreateWhen;
    fileDetails.modWhen = pRecord->recModWhen;
    fileDetails.archiveWhen = pRecord->recArchiveWhen;

    err = NuAddRecord(pOutArchive, &fileDetails, &newRecordIdx);
    if (err != kNuErrNone) {
        fprintf(stderr, "ERROR: NuAddRecord failed (err=%d)\n", err);
        goto bail;
    }

    /*
     * Copy the threads.
     */
    if (flags & kFlagReverseThreads) {
        for (idx = numThreads-1; idx >= 0; idx--) {
            pThread = NuGetThread(pRecord, idx);
            assert(pThread != nil);

            err = CopyThread(pInArchive, pOutArchive, flags, pThread,
                    newRecordIdx);
            if (err != kNuErrNone)
                goto bail;
        }
    } else {
        for (idx = 0; idx < numThreads; idx++) {
            pThread = NuGetThread(pRecord, idx);
            assert(pThread != nil);

            err = CopyThread(pInArchive, pOutArchive, flags, pThread,
                    newRecordIdx);
            if (err != kNuErrNone)
                goto bail;
        }
    }

bail:
    return err;
}


/*
 * Launder an archive from inFile to outFile.
 *
 * Returns 0 on success, nonzero on failure.
 */
int
LaunderArchive(const char* inFile, const char* outFile, NuValue compressMethod,
    long flags)
{
    NuError err = kNuErrNone;
    NuArchive* pInArchive = nil;
    NuArchive* pOutArchive = nil;
    const NuMasterHeader* pMasterHeader;
    NuRecordIdx recordIdx;
    long idx, flushStatus;

    err = NuOpenRO(inFile, &pInArchive);
    if (err != kNuErrNone) {
        fprintf(stderr, "ERROR: couldn't open input archive '%s' (err=%d)\n",
            inFile, err);
        goto bail;
    }
    err = NuOpenRW(outFile, kTempFile, kNuOpenCreat|kNuOpenExcl, &pOutArchive);
    if (err != kNuErrNone) {
        fprintf(stderr, "ERROR: couldn't open output archive '%s' (err=%d)\n",
            outFile, err);
        goto bail;
    }

    /* turn off "mimic GSHK" */
    err = NuSetValue(pInArchive, kNuValueMimicSHK, true);
    if (err != kNuErrNone) {
        fprintf(stderr, "ERROR: unable to disable GSHK quirks (err=%d)\n",
            err);
        goto bail;
    }

    /* allow duplicates, in case the original archive has them */
    err = NuSetValue(pOutArchive, kNuValueAllowDuplicates, true);
    if (err != kNuErrNone) {
        fprintf(stderr, "ERROR: couldn't allow duplicates (err=%d)\n", err);
        goto bail;
    }

    /* set the compression method */
    err = NuSetValue(pOutArchive, kNuValueDataCompression, compressMethod);
    if (err != kNuErrNone) {
        fprintf(stderr, "ERROR: unable to set compression (err=%d)\n",
            err);
        goto bail;
    }

    if (flags & kFlagUseTmp) {
        err = NuSetValue(pOutArchive, kNuValueModifyOrig, false);
        if (err != kNuErrNone) {
            fprintf(stderr,
                "ERROR: couldn't disable modify orig (err=%d)\n", err);
            goto bail;
        }
    }

    err = NuGetMasterHeader(pInArchive, &pMasterHeader);
    if (err != kNuErrNone) {
        fprintf(stderr, "ERROR: couldn't get master header (err=%d)\n", err);
        goto bail;
    }

    /*
     * Iterate through the set of records.
     */
    for (idx = 0; idx < (int)pMasterHeader->mhTotalRecords; idx++) {
        err = NuGetRecordIdxByPosition(pInArchive, idx, &recordIdx);
        if (err != kNuErrNone) {
            fprintf(stderr, "ERROR: couldn't get record #%ld (err=%d)\n",
                idx, err);
            goto bail;
        }

        err = CopyRecord(pInArchive, pOutArchive, flags, recordIdx);
        if (err != kNuErrNone)
            goto bail;

        /*
         * If "frequent abort" is set, abort what we just did and redo it.
         */
        if (flags & kFlagFrequentAbort) {
            /*printf("(abort)\n");*/
            err = NuAbort(pOutArchive);
            if (err != kNuErrNone) {
                fprintf(stderr, "ERROR: abort failed (err=%d)\n", err);
                goto bail;
            }

            err = CopyRecord(pInArchive, pOutArchive, flags, recordIdx);
            if (err != kNuErrNone)
                goto bail;

        }

        /*
         * If "frequent abort" or "frequent flush" is set, flush after
         * each record is copied.
         */
        if ((flags & kFlagFrequentAbort) || (flags & kFlagFrequentFlush)) {
            /*printf("(flush)\n");*/
            err = NuFlush(pOutArchive, &flushStatus);
            if (err != kNuErrNone) {
                fprintf(stderr,
                    "ERROR: flush failed (err=%d, status=0x%04lx)\n",
                    err, flushStatus);
                goto bail;
            }
        }
    }

    /* first and only flush if frequent-flushing wasn't enabled */
    err = NuFlush(pOutArchive, &flushStatus);
    if (err != kNuErrNone) {
        fprintf(stderr, "ERROR: flush failed (err=%d, status=0x%04lx)\n",
            err, flushStatus);
        goto bail;
    }

bail:
    if (pInArchive != nil)
        NuClose(pInArchive);
    if (pOutArchive != nil) {
        if (err != kNuErrNone)
            NuAbort(pOutArchive);
        NuClose(pOutArchive);   /* flush pending changes and close */
    }
    return (err != kNuErrNone);
}



/*
 * Can't count on having getopt on non-UNIX platforms, so just use this
 * quick version instead.  May not work exactly like getopt(), but it
 * does everything we need here.
 */
int myoptind = 0;
char* myoptarg = nil;
const char* curchar = nil;
int skipnext = false;

int
mygetopt(int argc, char** argv, const char* optstr)
{
    if (!myoptind) {
        myoptind = 1;
        if (argc <= 1)
            return EOF;
        curchar = argv[myoptind];
        if (*curchar != '-')
            return EOF;
    }

    curchar++;
    if (*curchar == '\0') {
        myoptind++;
        if (skipnext)
            myoptind++;
        if (myoptind >= argc)
            return EOF;
        curchar = argv[myoptind];
        if (*curchar != '-')
            return EOF;
        curchar++;
    }

    while (*optstr != '\0') {
        if (*optstr == *curchar) {
            /*printf("MATCHED '%c'\n", *optstr);*/
            if (*(optstr+1) == ':') {
                skipnext = true;
                myoptarg = argv[myoptind+1];
                /*printf("ATTACHED '%s'\n", myoptarg);*/
            }
            return *curchar;
        }

        optstr++;
    }

    fprintf(stderr, "Unrecognized option '%c'\n", *curchar);
    return *curchar;
}

/*
 * Print usage info.
 */
void
Usage(const char* argv0)
{
    fprintf(stderr, "Usage: %s [-crfat] [-m method] infile.shk outfile.shk\n",
        argv0);
    fprintf(stderr, "\t-c : copy only, does not recompress data\n");
    fprintf(stderr, "\t-r : copy threads in reverse order to test ordering\n");
    fprintf(stderr, "\t-f : call Flush frequently to reduce memory usage\n");
    fprintf(stderr, "\t-a : exercise nufxlib Abort code frequently\n");
    fprintf(stderr, "\t-t : write to temp file instead of directly to outfile.shk\n");
    fprintf(stderr,
        "\t[method] is one of {sq,lzw1,lzw2,lzc12,lzc16,deflate,bzip2}\n");
    fprintf(stderr, "\tIf not specified, method defaults to lzw2\n");
}

/*
 * Grab the name of an archive to read.
 */
int
main(int argc, char** argv)
{
    NuValue compressMethod = kNuCompressLZW2;
    long major, minor, bug;
    const char* pBuildDate;
    long flags = 0;
    int errorFlag;
    int ic;
    int cc;

    (void) NuGetVersion(&major, &minor, &bug, &pBuildDate, nil);
    printf("Using NuFX lib %ld.%ld.%ld built on or after %s\n",
        major, minor, bug, pBuildDate);

    errorFlag = false;
    while ((ic = mygetopt(argc, argv, "crfatm:")) != EOF) {
        switch (ic) {
        case 'c':   flags |= kFlagCopyOnly;         break;
        case 'r':   flags |= kFlagReverseThreads;   break;
        case 'f':   flags |= kFlagFrequentFlush;    break;
        case 'a':   flags |= kFlagFrequentAbort;    break;
        case 't':   flags |= kFlagUseTmp;           break;
        case 'm':
            {
                struct {
                    const char* str;
                    NuValue val;
                    NuFeature feature;
                } methods[] = {
                    { "sq",      kNuCompressSQ,      kNuFeatureCompressSQ },
                    { "lzw1",    kNuCompressLZW1,    kNuFeatureCompressLZW },
                    { "lzw2",    kNuCompressLZW2,    kNuFeatureCompressLZW },
                    { "lzc12",   kNuCompressLZC12,   kNuFeatureCompressLZC },
                    { "lzc16",   kNuCompressLZC16,   kNuFeatureCompressLZC },
                    { "deflate", kNuCompressDeflate, kNuFeatureCompressDeflate},
                    { "bzip2",   kNuCompressBzip2,   kNuFeatureCompressBzip2 },
                };
                char* methodStr = myoptarg;
                int i;

                for (i = 0; i < NELEM(methods); i++) {
                    if (strcmp(methods[i].str, methodStr) == 0) {
                        compressMethod = methods[i].val;
                        break;
                    }
                }
                if (i == NELEM(methods)) {
                    fprintf(stderr, "ERROR: unknown method '%s'\n", methodStr);
                    errorFlag++;
                    break;
                }
                if (NuTestFeature(methods[i].feature) != kNuErrNone) {
                    fprintf(stderr,
                        "ERROR: compression method '%s' not supported\n",
                        methodStr);
                    errorFlag++;
                    break;
                }
            }
            break;
        default:
            errorFlag++;
            break;
        }
    }

    if (errorFlag || argc != myoptind+2) {
        Usage(argv[0]);
        exit(2);
    }

    cc = LaunderArchive(argv[myoptind], argv[myoptind+1], compressMethod,flags);

    if (cc == 0)
        printf("Success!\n");
    else
        printf("Failed.\n");
    exit(cc != 0);
}

