/*
 * NuFX archive manipulation library
 * Copyright (C) 2000-2007 by Andy McFadden, All Rights Reserved.
 * This is free software; you can redistribute it and/or modify it under the
 * terms of the BSD License, see the file COPYING.LIB.
 *
 * Test basic features of the library.  Run this without arguments.
 */
#include <stdio.h>
#include <ctype.h>
#include "NufxLib.h"
#include "Common.h"

#define kTestArchive    "nlbt.shk"
#define kTestTempFile   "nlbt.tmp"

#define kNumEntries     3   /* how many records are we going to add? */
#define kTestEntryBytes     "bytes"
#define kTestEntryBytesUPPER "BYTES"
#define kTestEntryEnglish   "English"
#define kTestEntryLong      "three|is a fairly long filename, complete with" \
                            "punctuation and other nifty/bad stuff"
#define kLocalFssep     '|'

/*
 * Globals.
 */
char gSuppressError = false;
#define FAIL_OK     gSuppressError = true;
#define FAIL_BAD    gSuppressError = false;


/*
 * ===========================================================================
 *      Helper functions
 * ===========================================================================
 */

/*
 * Get a single character of input from the user.
 */
static char
TGetReplyChar(char defaultReply)
{
    char tmpBuf[32];

    if (fgets(tmpBuf, sizeof(tmpBuf), stdin) == nil)
        return defaultReply;
    if (tmpBuf[0] == '\n' || tmpBuf[0] == '\r')
        return defaultReply;

    return tmpBuf[0];
}

NuError
AddSimpleRecord(NuArchive* pArchive, const char* filename,
    NuRecordIdx* pRecordIdx)
{
    NuFileDetails fileDetails;

    memset(&fileDetails, 0, sizeof(fileDetails));
    fileDetails.storageName = filename;
    fileDetails.fileSysInfo = kLocalFssep;
    fileDetails.access = kNuAccessUnlocked;

    return NuAddRecord(pArchive, &fileDetails, pRecordIdx);
}


/*
 * Display error messages... or not.
 */
NuResult
ErrorMessageHandler(NuArchive* pArchive, void* vErrorMessage)
{
    const NuErrorMessage* pErrorMessage = (const NuErrorMessage*) vErrorMessage;

    if (gSuppressError)
        return kNuOK;

    if (pErrorMessage->isDebug) {
        fprintf(stderr, "%sNufxLib says: [%s:%d %s] %s\n",
            pArchive == nil ? "GLOBAL>" : "",
            pErrorMessage->file, pErrorMessage->line, pErrorMessage->function,
            pErrorMessage->message);
    } else {
        fprintf(stderr, "%sNufxLib says: %s\n",
            pArchive == nil ? "GLOBAL>" : "",
            pErrorMessage->message);
    }

    return kNuOK;
}

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
 * ===========================================================================
 *      Tests
 * ===========================================================================
 */

/*
 * Make sure the flags that control how we open the file work right,
 * and verify that we handle existing zero-byte archive files correctly.
 */
int
Test_OpenFlags(void)
{
    NuError err;
    FILE* fp = nil;
    NuArchive* pArchive = nil;

    printf("... open zero-byte existing\n");
    fp = fopen(kTestArchive, kNuFileOpenWriteTrunc);
    if (fp == nil) {
        perror("fopen kTestArchive");
        goto failed;
    }
    fclose(fp);
    fp = nil;

    FAIL_OK;
    err = NuOpenRW(kTestArchive, kTestTempFile, kNuOpenCreat|kNuOpenExcl,
            &pArchive);
    FAIL_BAD;
    if (err == kNuErrNone) {
        fprintf(stderr, "ERROR: file opened when it shouldn't have\n");
        goto failed;
    }

    err = NuOpenRW(kTestArchive, kTestTempFile, kNuOpenCreat, &pArchive);
    if (err != kNuErrNone) {
        fprintf(stderr, "ERROR: file didn't open when it should have\n");
        goto failed;
    }

    err = NuClose(pArchive);
    if (err != kNuErrNone) {
        fprintf(stderr, "ERROR: close failed\n");
        goto failed;
    }
    pArchive = nil;

    if (access(kTestArchive, F_OK) == 0) {
        fprintf(stderr, "ERROR: archive should have been removed but wasn't\n");
        goto failed;
    }

    return 0;

failed:
    if (pArchive != nil) {
        NuAbort(pArchive);
        NuClose(pArchive);
    }
    return -1;
}


/*
 * Add some files to the archive.  These will be used by later tests.
 */
int
Test_AddStuff(NuArchive* pArchive)
{
    NuError err;
    uchar* buf = nil;
    NuDataSource* pDataSource = nil;
    NuRecordIdx recordIdx;
    long status;
    int i;
    static const char* testMsg =
        "This is a nice test message that has linefeeds in it so we can\n"
        "see if the line conversion stuff is actually doing anything at\n"
        "all.  It's certainly nice to know that everything works the way\n"
        "it's supposed to, which I suppose is why we have this nifty test\n"
        "program available.  It sure would be nice if everybody tested\n"
        "there code, but where would Microsoft be without endless upgrades\n"
        "and service packs?  Bugs are what America was built on, and\n"
        "anybody who says otherwise is a pinko commie lowlife.  Verily.\n";

    printf("... add 'bytes' record\n");
    buf = malloc(131072);
    if (buf == nil)
        goto failed;
    for (i = 0; i < 131072; i++)
        *(buf+i) = i & 0xff;

    FAIL_OK;
    err = NuCreateDataSourceForBuffer(kNuThreadFormatUncompressed,
            0, nil, 0, 131072, FreeCallback, &pDataSource);
    FAIL_BAD;
    if (err == kNuErrNone) {
        fprintf(stderr, "ERROR: that should've failed!\n");
        goto failed;
    }

    /*
     * Create a data source for the big batch of bytes.
     */
    err = NuCreateDataSourceForBuffer(kNuThreadFormatUncompressed,
            0, buf, 0, 131072, FreeCallback, &pDataSource);
    if (err != kNuErrNone) {
        fprintf(stderr,
            "ERROR: 'bytes' data source create failed (err=%d)\n", err);
        goto failed;
    }
    buf = nil;  /* now owned by library */

    err = AddSimpleRecord(pArchive, kTestEntryBytes, &recordIdx);
    if (err != kNuErrNone) {
        fprintf(stderr, "ERROR: 'bytes' record failed (err=%d)\n", err);
        goto failed;
    }

    err = NuAddThread(pArchive, recordIdx, kNuThreadIDDataFork, pDataSource,
            nil);
    if (err != kNuErrNone) {
        fprintf(stderr, "ERROR: 'bytes' thread add failed (err=%d)\n", err);
        goto failed;
    }
    pDataSource = nil;  /* now owned by library */


    /*
     * Create a data source for our lovely text message.
     */
    printf("... add 'English' record\n");
    err = NuCreateDataSourceForBuffer(kNuThreadFormatUncompressed,
            0, (const uchar*)testMsg, 0, strlen(testMsg), nil, &pDataSource);
    if (err != kNuErrNone) {
        fprintf(stderr,
            "ERROR: 'English' source create failed (err=%d)\n", err);
        goto failed;
    }

    FAIL_OK;
    err = NuAddThread(pArchive, recordIdx, kNuThreadIDDataFork, pDataSource,
            nil);
    FAIL_BAD;
    if (err == kNuErrNone) {
        fprintf(stderr, "ERROR: 'English' add should've conflicted!\n");
        goto failed;
    }

    FAIL_OK;
    err = AddSimpleRecord(pArchive, kTestEntryBytes, &recordIdx);
    FAIL_BAD;
    if (err == kNuErrNone) {
        fprintf(stderr, "ERROR: duplicates not allowed, should've failed\n");
        goto failed;
    }

    err = AddSimpleRecord(pArchive, kTestEntryEnglish, &recordIdx);
    if (err != kNuErrNone) {
        fprintf(stderr, "ERROR: 'English' record failed (err=%d)\n", err);
        goto failed;
    }

    err = NuAddThread(pArchive, recordIdx, kNuThreadIDDataFork, pDataSource,
            nil);
    if (err != kNuErrNone) {
        fprintf(stderr, "ERROR: 'English' thread add failed (err=%d)\n", err);
        goto failed;
    }
    pDataSource = nil;  /* now owned by library */


    /*
     * Create an empty file with a rather non-empty name.
     */
    printf("... add 'long' record\n");
    err = NuCreateDataSourceForBuffer(kNuThreadFormatUncompressed,
            0, nil, 0, 0, nil, &pDataSource);
    if (err != kNuErrNone) {
        fprintf(stderr,
            "ERROR: 'English' source create failed (err=%d)\n", err);
        goto failed;
    }

    err = AddSimpleRecord(pArchive, kTestEntryLong, &recordIdx);
    if (err != kNuErrNone) {
        fprintf(stderr, "ERROR: 'long' record failed (err=%d)\n", err);
        goto failed;
    }

    err = NuAddThread(pArchive, recordIdx, kNuThreadIDRsrcFork, pDataSource,
            nil);
    if (err != kNuErrNone) {
        fprintf(stderr, "ERROR: 'long' thread add failed (err=%d)\n", err);
        goto failed;
    }
    pDataSource = nil;  /* now owned by library */


    /*
     * Flush changes.
     */
    err = NuFlush(pArchive, &status);
    if (err != kNuErrNone) {
        fprintf(stderr,"ERROR: couldn't flush after add (err=%d, status=%ld)\n",
            err, status);
        goto failed;
    }

    /*
     * Flush again; should succeed since it doesn't have to do anything.
     */
    err = NuFlush(pArchive, &status);
    if (err != kNuErrNone) {
        fprintf(stderr,"ERROR: second add flush failed (err=%d, status=%ld)\n",
            err, status);
        goto failed;
    }

    return 0;
failed:
    if (pDataSource != nil)
        NuFreeDataSource(pDataSource);
    if (buf != nil)
        free(buf);
    return -1;
}


/*
 * Make sure that what we're seeing makes sense.
 */
NuResult
TestContentsCallback(NuArchive* pArchive, void* vpRecord)
{
    const NuRecord* pRecord = (NuRecord*) vpRecord;

    if (strcmp(pRecord->filename, kTestEntryBytes) == 0 ||
        strcmp(pRecord->filename, kTestEntryEnglish) == 0 ||
        strcmp(pRecord->filename, kTestEntryLong) == 0)
    {
        return kNuOK;
    }

    fprintf(stderr, "ERROR: found mystery entry '%s'\n", pRecord->filename);
    return kNuAbort;
}


/*
 * Verify that the contents look about right.
 */
int
Test_Contents(NuArchive* pArchive)
{
    NuError err;
    long posn;
    NuRecordIdx recordIdx;
    const NuRecord* pRecord;
    int cc;

    /*
     * First, do it with a callback.
     */
    err = NuContents(pArchive, TestContentsCallback);
    if (err != kNuErrNone)
        goto failed;

    /*
     * Now step through the records with get-by-position and verify that
     * they're in the expected order.
     */
    for (posn = 0; posn < kNumEntries; posn++) {
        err = NuGetRecordIdxByPosition(pArchive, posn, &recordIdx);
        if (err != kNuErrNone) {
            fprintf(stderr, "ERROR: couldn't get record #%ld (err=%d)\n",
                posn, err);
            goto failed;
        }

        err = NuGetRecord(pArchive, recordIdx, &pRecord);
        if (err != kNuErrNone) {
            fprintf(stderr, "ERROR: couldn't get record index %ld (err=%d)\n",
                recordIdx, err);
            goto failed;
        }
        assert(pRecord != nil);

        switch (posn) {
        case 0:
            cc = strcmp(pRecord->filename, kTestEntryBytes);
            break;
        case 1:
            cc = strcmp(pRecord->filename, kTestEntryEnglish);
            break;
        case 2:
            cc = strcmp(pRecord->filename, kTestEntryLong);
            if (!cc)
                cc = !(pRecord->recStorageType == kNuStorageExtended);
            break;
        default:
            fprintf(stderr, "ERROR: somebody forgot to put a case here (%ld)\n",
                posn);
            cc = -1;
        }

        if (cc) {
            fprintf(stderr, "ERROR: got '%s' for %ld (%ld), not expected\n",
                pRecord->filename, posn, recordIdx);
            goto failed;
        }
    }

    /*
     * Read one more past the end, should fail.
     */
    FAIL_OK;
    err = NuGetRecordIdxByPosition(pArchive, posn, &recordIdx);
    FAIL_BAD;
    if (err == kNuErrNone) {
        fprintf(stderr, "ERROR: too many records (%ld was ok)\n", posn);
        goto failed;
    }

    return 0;
failed:
    return -1;
}


/*
 * Selection callback filter for "test".  This gets called once per record.
 */
NuResult
VerifySelectionCallback(NuArchive* pArchive, void* vpProposal)
{
    NuError err;
    const NuSelectionProposal* pProposal = vpProposal;
    long count;

    if (pProposal->pRecord == nil || pProposal->pThread == nil ||
        pProposal->pRecord->filename == nil)
    {
        fprintf(stderr, "ERROR: unexpected nil in proposal\n");
        goto failed;
    }

    err = NuGetExtraData(pArchive, (void**) &count);
    if (err != kNuErrNone) {
        fprintf(stderr, "ERROR: unable to get extra data (err=%d)\n", err);
        goto failed;
    }

    count++;

    err = NuSetExtraData(pArchive, (void*) count);
    if (err != kNuErrNone) {
        fprintf(stderr, "ERROR: unable to inc extra data (err=%d)\n", err);
        goto failed;
    }

    return kNuOK;
failed:
    return kNuAbort;
}

/*
 * Verify the archive contents.
 */
int
Test_Verify(NuArchive* pArchive)
{
    NuError err;
    long count;

    printf("... verifying CRCs\n");

    if (NuSetSelectionFilter(pArchive, VerifySelectionCallback) ==
        kNuInvalidCallback)
    {
        fprintf(stderr, "ERROR: unable to set selection filter\n");
        goto failed;
    }

    err = NuSetExtraData(pArchive, (void*) 0);
    if (err != kNuErrNone) {
        fprintf(stderr, "ERROR: unable to set extra data (err=%d)\n", err);
        goto failed;
    }

    err = NuTest(pArchive);
    if (err != kNuErrNone) {
        fprintf(stderr, "ERROR: verify failed (err=%d)\n", err);
        goto failed;
    }

    err = NuGetExtraData(pArchive, (void**) &count);
    if (err != kNuErrNone) {
        fprintf(stderr, "ERROR: last extra data get failed (err=%d)\n", err);
        goto failed;
    }

    if (count != kNumEntries) {
        fprintf(stderr, "ERROR: verified %ld when expecting %d\n", count,
            kNumEntries);
        goto failed;
    }

    return 0;
failed:
    return -1;
}

/*
 * Extract stuff.
 */
int
Test_Extract(NuArchive* pArchive)
{
    NuError err;
    NuRecordIdx recordIdx;
    const NuRecord* pRecord;
    const NuThread* pThread;
    NuDataSink* pDataSink = nil;
    uchar* buf = nil;

    printf("... extracting files\n");

    /*
     * Tell it the current system uses CRLF, so it'll bloat up when we do
     * a text conversion.
     */
    err = NuSetValue(pArchive, kNuValueEOL, kNuEOLCRLF);

    /*
     * Extract "bytes".
     */
    err = NuGetRecordIdxByName(pArchive, kTestEntryBytesUPPER, &recordIdx);
    if (err != kNuErrNone) {
        fprintf(stderr, "ERROR: couldn't find '%s' (err=%d)\n", kTestEntryBytes,
            err);
        goto failed;
    }
    err = NuGetRecord(pArchive, recordIdx, &pRecord);
    if (err != kNuErrNone) {
        fprintf(stderr, "ERROR: couldn't get record index %ld (err=%d)\n",
            recordIdx, err);
        goto failed;
    }
    assert(pRecord != nil);

    /* we're not using ShrinkIt compat mode, so there should not be a comment */
    pThread = NuGetThread(pRecord, 1);
    assert(pThread != nil);
    if (NuGetThreadID(pThread) != kNuThreadIDDataFork) {
        fprintf(stderr, "ERROR: 'bytes' had unexpected threadID 0x%08lx\n",
            NuGetThreadID(pThread));
        goto failed;
    }

    buf = malloc(pThread->actualThreadEOF);
    if (buf == nil) {
        fprintf(stderr, "ERROR: malloc(%ld) failed\n",pThread->actualThreadEOF);
        goto failed;
    }

    /* 
     * Try to extract it with text conversion off.
     */
    err = NuCreateDataSinkForBuffer(true, kNuConvertOff, buf,
            pThread->actualThreadEOF, &pDataSink);
    if (err != kNuErrNone) {
        fprintf(stderr, "ERROR: couldn't create data sink (err=%d)\n", err);
        goto failed;
    }

    err = NuExtractThread(pArchive, pThread->threadIdx, pDataSink);
    if (err != kNuErrNone) {
        fprintf(stderr, "ERROR: couldn't extract 'bytes' (off) (err=%d)\n",
            err);
        goto failed;
    }
    NuFreeDataSink(pDataSink);
    pDataSink = nil;

    /*
     * Try to extract with "on" conversion, which should fail because the
     * buffer is too small.
     */
    err = NuCreateDataSinkForBuffer(true, kNuConvertOn, buf,
            pThread->actualThreadEOF, &pDataSink);
    if (err != kNuErrNone) {
        fprintf(stderr, "ERROR: couldn't create data sink (err=%d)\n", err);
        goto failed;
    }

    FAIL_OK;
    err = NuExtractThread(pArchive, pThread->threadIdx, pDataSink);
    FAIL_BAD;
    if (err == kNuErrNone) {
        fprintf(stderr, "ERROR: managed to extract bloated 'bytes'?\n");
        goto failed;
    }
    NuFreeDataSink(pDataSink);
    pDataSink = nil;

    /*
     * Try to extract with "auto" conversion, which should conclude that
     * the input is text and not try to convert.
     */
    err = NuCreateDataSinkForBuffer(true, kNuConvertAuto, buf,
            pThread->actualThreadEOF, &pDataSink);
    if (err != kNuErrNone) {
        fprintf(stderr, "ERROR: couldn't create data sink (err=%d)\n", err);
        goto failed;
    }

    err = NuExtractThread(pArchive, pThread->threadIdx, pDataSink);
    if (err != kNuErrNone) {
        fprintf(stderr, "ERROR: couldn't extract 'bytes' (auto) (err=%d)\n",
            err);
        goto failed;
    }
    NuFreeDataSink(pDataSink);
    pDataSink = nil;



    free(buf);
    buf = nil;



    /*
     * Extract "English".
     */
    err = NuGetRecordIdxByName(pArchive, kTestEntryEnglish, &recordIdx);
    if (err != kNuErrNone) {
        fprintf(stderr, "ERROR: couldn't find '%s' (err=%d)\n",
            kTestEntryEnglish, err);
        goto failed;
    }
    err = NuGetRecord(pArchive, recordIdx, &pRecord);
    if (err != kNuErrNone) {
        fprintf(stderr, "ERROR: couldn't get record index %ld (err=%d)\n",
            recordIdx, err);
        goto failed;
    }
    assert(pRecord != nil);

    /* we're not using ShrinkIt compat mode, so there should not be a comment */
    pThread = NuGetThread(pRecord, 1);
    assert(pThread != nil);
    if (NuGetThreadID(pThread) != kNuThreadIDDataFork) {
        fprintf(stderr, "ERROR: 'English' had unexpected threadID 0x%08lx\n",
            NuGetThreadID(pThread));
        goto failed;
    }

    buf = malloc(pThread->actualThreadEOF);
    if (buf == nil) {
        fprintf(stderr, "ERROR: malloc(%ld) failed\n",pThread->actualThreadEOF);
        goto failed;
    }

    /* 
     * Try to extract it with text conversion off.
     */
    err = NuCreateDataSinkForBuffer(true, kNuConvertOff, buf,
            pThread->actualThreadEOF, &pDataSink);
    if (err != kNuErrNone) {
        fprintf(stderr, "ERROR: couldn't create data sink (err=%d)\n", err);
        goto failed;
    }

    err = NuExtractThread(pArchive, pThread->threadIdx, pDataSink);
    if (err != kNuErrNone) {
        fprintf(stderr, "ERROR: couldn't extract 'bytes' (off) (err=%d)\n",
            err);
        goto failed;
    }
    NuFreeDataSink(pDataSink);
    pDataSink = nil;

    /*
     * Try to extract with "auto" conversion, which should fail because the
     * buffer is too small, and the input looks like text.
     */
    err = NuCreateDataSinkForBuffer(true, kNuConvertAuto, buf,
            pThread->actualThreadEOF, &pDataSink);
    if (err != kNuErrNone) {
        fprintf(stderr, "ERROR: couldn't create data sink (err=%d)\n", err);
        goto failed;
    }

    FAIL_OK;
    err = NuExtractThread(pArchive, pThread->threadIdx, pDataSink);
    FAIL_BAD;
    if (err == kNuErrNone) {
        fprintf(stderr, "ERROR: managed to extract bloated 'English'?\n");
        goto failed;
    }
    NuFreeDataSink(pDataSink);
    pDataSink = nil;



    /*Free(buf);*/
    /*buf = nil;*/



    /*
     * Extract "long" (which is zero bytes).
     */
    err = NuGetRecordIdxByName(pArchive, kTestEntryLong, &recordIdx);
    if (err != kNuErrNone) {
        fprintf(stderr, "ERROR: couldn't find '%s' (err=%d)\n",
            kTestEntryLong, err);
        goto failed;
    }
    err = NuGetRecord(pArchive, recordIdx, &pRecord);
    if (err != kNuErrNone) {
        fprintf(stderr, "ERROR: couldn't get record index %ld (err=%d)\n",
            recordIdx, err);
        goto failed;
    }
    assert(pRecord != nil);

    /* we're not using ShrinkIt compat mode, so there should not be a comment */
    pThread = NuGetThread(pRecord, 1);
    assert(pThread != nil);
    if (NuGetThreadID(pThread) != kNuThreadIDRsrcFork) {
        fprintf(stderr, "ERROR: 'Long' had unexpected threadID 0x%08lx\n",
            NuGetThreadID(pThread));
        goto failed;
    }

    /* 
     * Try it with text conversion on; shouldn't matter.
     */
    err = NuCreateDataSinkForBuffer(true, kNuConvertOn, buf,
            1, &pDataSink);
    if (err != kNuErrNone) {
        fprintf(stderr, "ERROR: couldn't create data sink (err=%d)\n", err);
        goto failed;
    }

    err = NuExtractThread(pArchive, pThread->threadIdx, pDataSink);
    if (err != kNuErrNone) {
        fprintf(stderr, "ERROR: couldn't extract 'Long' (off) (err=%d)\n",
            err);
        goto failed;
    }
    NuFreeDataSink(pDataSink);
    pDataSink = nil;



    free(buf);
    buf = nil;



    return 0;
failed:
    if (buf != nil)
        free(buf);
    if (pDataSink != nil)
        (void) NuFreeDataSink(pDataSink);
    return -1;
}

/*
 * Delete the first and last records.  Does *not* flush the archive.
 */
int
Test_Delete(NuArchive* pArchive)
{
    NuError err;
    NuRecordIdx recordIdx;
    const NuRecord* pRecord;
    const NuThread* pThread = nil;
    long count;
    int idx;

    printf("... deleting first and last\n");

    /*
     * Delete all threads from the first record ("bytes").
     */
    err = NuGetRecordIdxByPosition(pArchive, 0, &recordIdx);
    if (err != kNuErrNone) {
        fprintf(stderr, "ERROR: couldn't find #%d (err=%d)\n", 0, err);
        goto failed;
    }
    err = NuGetRecord(pArchive, recordIdx, &pRecord);
    if (err != kNuErrNone) {
        fprintf(stderr, "ERROR: couldn't get record index %ld (err=%d)\n",
            recordIdx, err);
        goto failed;
    }
    assert(pRecord != nil);
    assert(pRecord->recTotalThreads > 0);

    for (idx = 0; idx < (int)pRecord->recTotalThreads; idx++) {
        pThread = NuGetThread(pRecord, idx);
        assert(pThread != nil);

        err = NuDeleteThread(pArchive, pThread->threadIdx);
        if (err != kNuErrNone) {
            fprintf(stderr,
                "ERROR: couldn't delete thread #%d (%ld) (err=%d)\n",
                idx, recordIdx, err);
            goto failed;
        }
    }

    /* try to re-delete the same thread */
    assert(pThread != nil);
    FAIL_OK;
    err = NuDeleteThread(pArchive, pThread->threadIdx);
    FAIL_BAD;
    if (err == kNuErrNone) {
        fprintf(stderr, "ERROR: allowed to re-delete thread (%ld) (err=%d)\n",
            recordIdx, err);
        goto failed;
    }

    /* try to delete the modified record */
    FAIL_OK;
    err = NuDeleteRecord(pArchive, recordIdx);
    FAIL_BAD;
    if (err == kNuErrNone) {
        fprintf(stderr,
            "ERROR: able to delete modified record (%ld) (err=%d)\n",
            recordIdx, err);
        goto failed;
    }

    /*
     * Make sure the attr hasn't been updated yet.
     */
    err = NuGetAttr(pArchive, kNuAttrNumRecords, (unsigned long*) &count);
    if (count != kNumEntries) {
        fprintf(stderr, "ERROR: kNuAttrNumRecords %ld vs %d\n",
            count, kNumEntries);
        goto failed;
    }

    /*
     * Delete the last record ("long").
     */
    err = NuGetRecordIdxByPosition(pArchive, kNumEntries-1, &recordIdx);
    if (err != kNuErrNone) {
        fprintf(stderr, "ERROR: couldn't find #%d (err=%d)\n", 0, err);
        goto failed;
    }
    err = NuGetRecord(pArchive, recordIdx, &pRecord);
    if (err != kNuErrNone) {
        fprintf(stderr, "ERROR: couldn't get record index %ld (err=%d)\n",
            recordIdx, err);
        goto failed;
    }
    assert(pRecord != nil);

    /* grab the first thread before we whack the record */
    pThread = NuGetThread(pRecord, 0);
    assert(pThread != nil);

    err = NuDeleteRecord(pArchive, recordIdx);
    if (err != kNuErrNone) {
        fprintf(stderr, "ERROR: unable to delete record #%d (%ld) (err=%d)\n",
            kNumEntries-1, recordIdx, err);
        goto failed;
    }

    /* try to delete a thread from the deleted record */
    FAIL_OK;
    err = NuDeleteThread(pArchive, pThread->threadIdx);
    FAIL_BAD;
    if (err == kNuErrNone) {
        fprintf(stderr,
            "ERROR: allowed to delete from deleted (%ld) (err=%d)\n",
            pThread->threadIdx, err);
        goto failed;
    }

    return 0;
failed:
    return -1;
}


/*
 * Verify that the count in the master header has been updated.
 */
int
Test_MasterCount(NuArchive* pArchive, long expected)
{
    NuError err;
    const NuMasterHeader* pMasterHeader;

    printf("... checking master count\n");

    err = NuGetMasterHeader(pArchive, &pMasterHeader);
    if (err != kNuErrNone) {
        fprintf(stderr, "ERROR: couldn't get master header (err=%d)\n", err);
        goto failed;
    }

    if (pMasterHeader->mhTotalRecords != (ulong)expected) {
        fprintf(stderr, "ERROR: unexpected MH count (%ld vs %ld)\n",
            pMasterHeader->mhTotalRecords, expected);
        goto failed;
    }

    return 0;
failed:
    return -1;
}


/*
 * Run some tests.
 *
 * Returns 0 on success, -1 on error.
 */
int
DoTests(void)
{
    NuError err;
    NuArchive* pArchive = nil;
    long status;
    int cc, result = 0;
    char answer;

    /*
     * Make sure we're starting with a clean slate.
     */
    if (access(kTestArchive, F_OK) == 0) {
        printf("Test archive '%s' exists, remove (y/n)? ", kTestArchive);
        fflush(stdout);
        answer = TGetReplyChar('n');
        if (tolower(answer) != 'y')
            goto failed;
        cc = unlink(kTestArchive);
        if (cc < 0) {
            perror("unlink kTestArchive");
            goto failed;
        }
    }
    if (access(kTestTempFile, F_OK) == 0) {
        printf("Test temp file '%s' exists, remove (y/n)? ", kTestTempFile);
        fflush(stdout);
        answer = TGetReplyChar('n');
        if (tolower(answer) != 'y')
            goto failed;
        cc = unlink(kTestTempFile);
        if (cc < 0) {
            perror("unlink kTestTempFile");
            goto failed;
        }
    }

    /*
     * Test some of the open flags.
     */
    if (Test_OpenFlags() != 0)
        goto failed;

    /*
     * Create a new archive to play with.
     */
    err = NuOpenRW(kTestArchive, kTestTempFile, kNuOpenCreat|kNuOpenExcl,
            &pArchive);
    if (err != kNuErrNone) {
        fprintf(stderr, "ERROR: NuOpenRW failed (err=%d)\n", err);
        goto failed;
    }
    if (NuSetErrorMessageHandler(pArchive, ErrorMessageHandler) ==
        kNuInvalidCallback)
    {
        fprintf(stderr, "ERROR: couldn't set message handler\n");
        goto failed;
    }

    /*
     * Add some test entries.
     */
    if (Test_AddStuff(pArchive) != 0)
        goto failed;

    /*
     * Check the archive contents.
     */
    printf("... checking contents\n");
    if (Test_Contents(pArchive) != 0)
        goto failed;

    /*
     * Reopen it read-only.
     */
    printf("... reopening archive read-only\n");
    err = NuClose(pArchive);
    if (err != kNuErrNone) {
        fprintf(stderr, "ERROR: mid NuClose failed (err=%d)\n", err);
        goto failed;
    }
    pArchive = nil;

    err = NuOpenRO(kTestArchive, &pArchive);
    if (err != kNuErrNone) {
        fprintf(stderr, "ERROR: NuOpenRO failed (err=%d)\n", err);
        goto failed;
    }
    if (NuSetErrorMessageHandler(pArchive, ErrorMessageHandler) ==
        kNuInvalidCallback)
    {
        fprintf(stderr, "ERROR: couldn't set message handler\n");
        goto failed;
    }

    /*
     * Make sure the contents are still what we expect.
     */
    printf("... checking contents\n");
    if (Test_Contents(pArchive) != 0)
        goto failed;

    /*
     * Verify the archive contents.
     */
    if (Test_Verify(pArchive) != 0)
        goto failed;

    /*
     * Extract the files.
     */
    if (Test_Extract(pArchive) != 0)
        goto failed;

    /*
     * Reopen it read-write.
     */
    printf("... reopening archive read-write\n");
    err = NuClose(pArchive);
    if (err != kNuErrNone) {
        fprintf(stderr, "ERROR: late NuClose failed (err=%d)\n", err);
        goto failed;
    }
    pArchive = nil;

    err = NuOpenRW(kTestArchive, kTestTempFile, 0, &pArchive);
    if (err != kNuErrNone) {
        fprintf(stderr, "ERROR: re-NuOpenRW failed (err=%d)\n", err);
        goto failed;
    }
    if (NuSetErrorMessageHandler(pArchive, ErrorMessageHandler) ==
        kNuInvalidCallback)
    {
        fprintf(stderr, "ERROR: couldn't set message handler\n");
        goto failed;
    }

    /*
     * Contents shouldn't have changed.
     */
    printf("... checking contents\n");
    if (Test_Contents(pArchive) != 0)
        goto failed;

    /*
     * Test deletion.
     */
    if (Test_Delete(pArchive) != 0)
        goto failed;

    /*
     * Abort the changes and verify that nothing has changed.
     */
    printf("... aborting changes\n");
    err = NuAbort(pArchive);
    if (err != kNuErrNone) {
        fprintf(stderr, "ERROR: abort failed (err=%d)\n", err);
        goto failed;
    }

    printf("... checking contents\n");
    if (Test_Contents(pArchive) != 0)
        goto failed;

    /*
     * Delete them again.
     */
    if (Test_Delete(pArchive) != 0)
        goto failed;

    /*
     * Flush the deletions.  This should remove the first and last records.
     */
    err = NuFlush(pArchive, &status);
    if (err != kNuErrNone) {
        fprintf(stderr, "ERROR: flush failed (err=%d, status=%ld)\n",
            err, status);
        goto failed;
    }

    /*
     * Check count in master header.
     */
    if (Test_MasterCount(pArchive, kNumEntries-2) != 0)
        goto failed;

    /*
     * That's all, folks...
     */
    NuClose(pArchive);
    pArchive = nil;

    printf("... removing '%s'\n", kTestArchive);
    cc = unlink(kTestArchive);
    if (cc < 0) {
        perror("unlink kTestArchive");
        goto failed;
    }


leave:
    if (pArchive != nil) {
        NuAbort(pArchive);
        NuClose(pArchive);
    }
    return result;

failed:
    result = -1;
    goto leave;
}


/*
 * Crank away.
 */
int
main(void)
{
    long major, minor, bug;
    const char* pBuildDate;
    const char* pBuildFlags;
    int cc;

    (void) NuGetVersion(&major, &minor, &bug, &pBuildDate, &pBuildFlags);
    printf("Using NuFX library v%ld.%ld.%ld, built on or after\n"
           "  %s with [%s]\n\n",
        major, minor, bug, pBuildDate, pBuildFlags);

    if (NuSetGlobalErrorMessageHandler(ErrorMessageHandler) ==
        kNuInvalidCallback)
    {
        fprintf(stderr, "ERROR: can't set the global message handler");
        exit(1);
    }

    printf("... starting tests\n");

    cc = DoTests();

    printf("... tests ended, %s\n", cc == 0 ? "SUCCESS" : "FAILURE");
    exit(cc != 0);
}

