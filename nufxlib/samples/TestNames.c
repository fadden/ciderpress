/*
 * NuFX archive manipulation library
 * Copyright (C) 2000-2007 by Andy McFadden, All Rights Reserved.
 * This is free software; you can redistribute it and/or modify it under the
 * terms of the BSD License, see the file COPYING.LIB.
 *
 * Test local and storage names with Unicode and Mac OS Roman content.
 *
 * On Windows, opening files with fancy filenames requires UTF-16 and
 * special functions.  On Linux and Mac OS X we're just writing UTF-8 data,
 * so they don't really need to do anything special other than be 8-bit
 * clean.  NufxLib functions take UTF-8 strings, so on Windows we define
 * everything in UTF-16 and convert to UTF-8.  (We need the UTF-16 form so
 * we can use "wide" I/O functions to confirm that the file was created
 * with the correct name.)
 *
 * To see files with the correct appearance with "ls", you may need to
 * do something like:
 *
 *  % LC_ALL=en_US.UTF-8 ls
 *
 * (Many users set LC_ALL=POSIX to avoid GNU grep slowdowns and altered
 * sort ordering in ls.)
 */
#include <stdio.h>
#include <ctype.h>
#include "NufxLib.h"
#include "Common.h"

/*
 * Test filenames.
 *
 * The local filename (kTestArchive) contains non-MOR Unicode values
 * (two Japanese characters that Google Translate claims form the verb
 * "shrink").  The temp file name is similar.
 *
 * The entry name uses a mix of simple ASCII, CP1252 MOR, and
 * non-CP1252 MOR characters: fl ligature, double dagger, copyright symbol,
 * Apple logo (the latter of which doesn't have a glyph on Windows or Linux).
 * All of the characters have MOR translations.
 */
#ifdef USE_UTF16
const UNICHAR kTestArchive[]   = L"nlTest\u7e2e\u3080.shk";
const UNICHAR kTestEntryName[] = L"nl-test\u2013\ufb01_\u2021_\u00a9\uf8ff!";
const UNICHAR kTestTempFile[]  = L"nlTest\4e00\u6642\u30d5\u30a1\u30a4\u30eb.tmp";
#else
const UNICHAR kTestArchive[]   = "nlTest\xe7\xb8\xae\xe3\x82\x80.shk";
const UNICHAR kTestEntryName[] = "nl-test\xe2\x80\x93\xef\xac\x81_\xe2\x80\xa1_"
                                 "\xc2\xa9\xef\xa3\xbf!";
const UNICHAR kTestTempFile[]  = "nlTest\xe4\xb8\x80\xe6\x99\x82\xe3\x83\x95"
                                 "\xe3\x82\xa1\xe3\x82\xa4\xe3\x83\xab.tmp";
#endif

const UNICHAR kLocalFssep = '|';


/*
 * ===========================================================================
 *      Helper functions
 * ===========================================================================
 */

/*
 * Get a single character of input from the user.
 */
static char TGetReplyChar(char defaultReply)
{
    char tmpBuf[32];

    if (fgets(tmpBuf, sizeof(tmpBuf), stdin) == NULL)
        return defaultReply;
    if (tmpBuf[0] == '\n' || tmpBuf[0] == '\r')
        return defaultReply;

    return tmpBuf[0];
}

NuError AddSimpleRecord(NuArchive* pArchive, const char* fileNameMOR,
    NuRecordIdx* pRecordIdx)
{
    NuFileDetails fileDetails;

    memset(&fileDetails, 0, sizeof(fileDetails));
    fileDetails.storageNameMOR = fileNameMOR;
    fileDetails.fileSysInfo = kLocalFssep;
    fileDetails.access = kNuAccessUnlocked;

    return NuAddRecord(pArchive, &fileDetails, pRecordIdx);
}

/*
 * Display error messages... or not.
 */
NuResult ErrorMessageHandler(NuArchive* pArchive, void* vErrorMessage)
{
    const NuErrorMessage* pErrorMessage = (const NuErrorMessage*) vErrorMessage;

    //if (gSuppressError)
    //    return kNuOK;

    if (pErrorMessage->isDebug) {
        fprintf(stderr, "%sNufxLib says: [%s:%d %s] %s\n",
            pArchive == NULL ? "GLOBAL>" : "",
            pErrorMessage->file, pErrorMessage->line, pErrorMessage->function,
            pErrorMessage->message);
    } else {
        fprintf(stderr, "%sNufxLib says: %s\n",
            pArchive == NULL ? "GLOBAL>" : "",
            pErrorMessage->message);
    }

    return kNuOK;
}

#ifdef USE_UTF16
TODO - use _waccess, _wunlink, etc.
#else
int RemoveTestFile(const char* title, const char* fileName)
{
    char answer;

    if (access(fileName, F_OK) == 0) {
        printf("%s '%s' exists, remove (y/n)? ", title, fileName);
        fflush(stdout);
        answer = TGetReplyChar('n');
        if (tolower(answer) != 'y')
            return -1;
        if (unlink(fileName) < 0) {
            perror("unlink");
            return -1;
        }
    }
    return 0;
}
#endif

/*
 * Utility function that wraps NuConvertUNIToMOR, allocating a new
 * buffer to hold the converted string.  The caller must free the result.
 */
char* CopyUNIToMOR(const UNICHAR* stringUNI)
{
    size_t morLen;
    char* morBuf;

    morLen = NuConvertUNIToMOR(stringUNI, NULL, 0);
    if (morLen == (size_t) -1) {
        return NULL;
    }
    morBuf = (char*) malloc(morLen);
    (void) NuConvertUNIToMOR(stringUNI, morBuf, morLen);
    return morBuf;
}

/*
 * Utility function that wraps NuConvertMORToUNI, allocating a new
 * buffer to hold the converted string.  The caller must free the result.
 */
UNICHAR* CopyMORToUNI(const char* stringMOR)
{
    size_t uniLen;
    char* uniBuf;

    uniLen = NuConvertMORToUNI(stringMOR, NULL, 0);
    if (uniLen == (size_t) -1) {
        return NULL;
    }
    uniBuf = (UNICHAR*) malloc(uniLen);
    (void) NuConvertMORToUNI(stringMOR, uniBuf, uniLen);
    return uniBuf;
}


/*
 * ===========================================================================
 *      Tests
 * ===========================================================================
 */

void DumpMorString(const char* str)
{
    printf("(%d) ", (int) strlen(str));
    while (*str != '\0') {
        if (*str >= 0x20 && *str < 0x7f) {
            putchar(*str);
        } else {
            printf("\\x%02x", (uint8_t) *str);
        }
        str++;
    }
    putchar('\n');
}

void DumpUnicharString(const UNICHAR* str)
{
    printf("(%d) ", (int) strlen(str));
    while (*str != '\0') {
        if (*str >= 0x20 && *str < 0x7f) {
            putchar(*str);
        } else {
            if (sizeof(UNICHAR) == 1) {
                printf("\\x%02x", (uint8_t) *str);
            } else {
                printf("\\u%04x", (uint16_t) *str);
            }
        }
        str++;
    }
    putchar('\n');
}

/*
 * Some basic string conversion unit tests.
 *
 * TODO: test with short buffer, make sure we don't get partial code
 * points when converting to Unicode
 */
int TestStringConversion(void)
{
    static const char kMORTest[] = "test\xe0\xe9\xed\xf3\xfa#\xf0\xb0";

    size_t outLen;
    char morBuf[512];
    UNICHAR uniBuf[512];

    // convert test string to Unicode
    memset(uniBuf, 0xcc, sizeof(uniBuf));
    //printf("MOR: "); DumpMorString(kMORTest);

    outLen = NuConvertMORToUNI(kMORTest, NULL, 0);
    //printf("outLen is %u\n", (unsigned int) outLen);
    if (NuConvertMORToUNI(kMORTest, uniBuf, sizeof(uniBuf)) != outLen) {
        fprintf(stderr, "Inconsistent MORToUNI len\n");
        return -1;
    }
    //printf("UNI: "); DumpUnicharString(uniBuf);
    if (strlen(uniBuf) + 1 != outLen) {
        fprintf(stderr, "Expected length != actual length\n");
        return -1;
    }

    // convert Unicode back to MOR
    memset(morBuf, 0xcc, sizeof(morBuf));

    outLen = NuConvertUNIToMOR(uniBuf, NULL, 0);
    //printf("outLen is %u\n", (unsigned int) outLen);
    if (NuConvertUNIToMOR(uniBuf, morBuf, sizeof(morBuf)) != outLen) {
        fprintf(stderr, "Inconsistent UNIToMOR len\n");
        return -1;
    }
    //printf("MOR: "); DumpMorString(morBuf);
    if (strlen(morBuf) + 1 != outLen) {
        fprintf(stderr, "Expected length != actual length\n");
        return -1;
    }

    // check vs. original
    if (strcmp(kMORTest, morBuf) != 0) {
        fprintf(stderr, "Test string corrupted by double conversion\n");
        return -1;
    }

#ifdef USE_UTF16
    static const UNICHAR kNonMorUniStr[] = L"nlTest\u7e2e\u3080.shk";
    static const UNICHAR kBadUniStr[] = L"nlTest\u7e2e\x30";
#else
    static const UNICHAR kNonMorUniStr[] = "nlTest\xe7\xb8\xae\xe3\x82\x80.shk";
    static const UNICHAR kBadUniStr[] = "nlTest\x81\xe7";
#endif
    static const char kNonMorExpected[] = "nlTest??.shk";
    static const char kBadExpected[] = "nlTest??";

    NuConvertUNIToMOR(kNonMorUniStr, morBuf, sizeof(morBuf));
    if (strcmp(morBuf, kNonMorExpected) != 0) {
        fprintf(stderr, "Non-MOR string conversion failed\n");
        return -1;
    }

    NuConvertUNIToMOR(kBadUniStr, morBuf, sizeof(morBuf));
    if (strcmp(morBuf, kBadExpected) != 0) {
        fprintf(stderr, "Bad UNI string conversion failed\n");
        return -1;
    }

    printf("... string conversion tests successful\n");

    return 0;
}

/*
 * Create a new entry and give it a trivial data fork.
 */
int AddTestEntry(NuArchive* pArchive, const char* entryNameMOR)
{
    NuDataSource* pDataSource = NULL;
    NuRecordIdx recordIdx;
    static const char* kTestMsg = "Hello, world!\n";
    uint32_t status;
    NuError err;

    /*
     * Add our test entry.
     */
    err = AddSimpleRecord(pArchive, entryNameMOR, &recordIdx);
    if (err != kNuErrNone) {
        fprintf(stderr, "ERROR: add record failed (err=%d)\n", err);
        goto failed;
    }

    err = NuCreateDataSourceForBuffer(kNuThreadFormatUncompressed,
            0, (const uint8_t*)kTestMsg, 0, strlen(kTestMsg), NULL,
            &pDataSource);
    if (err != kNuErrNone) {
        fprintf(stderr, "ERROR: source create failed (err=%d)\n", err);
        goto failed;
    }

    err = NuAddThread(pArchive, recordIdx, kNuThreadIDDataFork, pDataSource,
            NULL);
    if (err != kNuErrNone) {
        fprintf(stderr, "ERROR: thread add failed (err=%d)\n", err);
        goto failed;
    }
    pDataSource = NULL;  /* now owned by library */

    /*
     * Flush changes.
     */
    err = NuFlush(pArchive, &status);
    if (err != kNuErrNone) {
        fprintf(stderr, "ERROR: couldn't flush after add (err=%d, status=%u)\n",
            err, status);
        goto failed;
    }

    return 0;
failed:
    if (pDataSource != NULL)
        NuFreeDataSource(pDataSource);
    return -1;
}

/*
 * Extract the file we created.
 */
int TestExtract(NuArchive* pArchive, const char* entryNameMOR)
{
    const NuRecord* pRecord;
    NuRecordIdx recordIdx;
    NuError err;

    err = NuGetRecordIdxByName(pArchive, entryNameMOR, &recordIdx);
    if (err != kNuErrNone) {
        fprintf(stderr, "ERROR: couldn't find '%s' (err=%d)\n",
            entryNameMOR, err);
        return -1;
    }
    err = NuGetRecord(pArchive, recordIdx, &pRecord);
    if (err != kNuErrNone) {
        fprintf(stderr, "ERROR: couldn't get record index %u (err=%d)\n",
            recordIdx, err);
        return -1;
    }
    assert(pRecord != NULL);

    const NuThread* pThread = NULL;
    uint32_t idx;
    for (idx = 0; idx < NuRecordGetNumThreads(pRecord); idx++) {
        pThread = NuGetThread(pRecord, idx);

        if (NuGetThreadID(pThread) == kNuThreadIDDataFork)
            break;
    }
    if (pThread == NULL) {
        fprintf(stderr, "ERROR: no data thread?\n");
        return -1;
    }

    /*
     * Prepare the output file.
     */
    UNICHAR* entryNameUNI = CopyMORToUNI(entryNameMOR);
    NuDataSink* pDataSink = NULL;
    err = NuCreateDataSinkForFile(true, kNuConvertOff, entryNameUNI,
            kLocalFssep, &pDataSink);
    if (err != kNuErrNone) {
        fprintf(stderr, "ERROR: unable to create data sink for file (err=%d)\n",
            err);
        free(entryNameUNI);
        return -1;
    }

    err = NuExtractThread(pArchive, pThread->threadIdx, pDataSink);
    if (err != kNuErrNone) {
        fprintf(stderr, "ERROR: extract failed (err=%d)\n", err);
        (void) NuFreeDataSink(pDataSink);
        free(entryNameUNI);
        return -1;
    }

    (void) NuFreeDataSink(pDataSink);

    printf("... confirming extraction of '%s'\n", entryNameUNI);
    if (access(entryNameUNI, F_OK) != 0) {
        fprintf(stderr, "ERROR: unable to read '%s' (err=%d)\n",
            entryNameUNI, errno);
        free(entryNameUNI);
        return -1;
    }

    if (unlink(entryNameUNI) < 0) {
        perror("unlink test entry");
        free(entryNameUNI);
        return -1;
    }

    free(entryNameUNI);
    return 0;
}

/*
 * Run some tests.
 *
 * Returns 0 on success, -1 on error.
 */
int DoTests(void)
{
    NuError err;
    NuArchive* pArchive = NULL;
    char* testEntryNameMOR = NULL;
    int result = 0;

    if (TestStringConversion() < 0) {
        goto failed;
    }

    /*
     * Make sure we're starting with a clean slate.
     */
    if (RemoveTestFile("Test archive", kTestArchive) < 0) {
        goto failed;
    }
    if (RemoveTestFile("Test temp file", kTestTempFile) < 0) {
        goto failed;
    }
    if (RemoveTestFile("Test entry", kTestEntryName) < 0) {
        goto failed;
    }

    testEntryNameMOR = CopyUNIToMOR(kTestEntryName);

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
     * Add a single entry.
     */
    if (AddTestEntry(pArchive, testEntryNameMOR) != 0) {
        goto failed;
    }

    printf("... checking presence of '%s' and '%s'\n",
        kTestArchive, kTestTempFile);

    if (access(kTestTempFile, F_OK) != 0) {
        /* in theory, NufxLib doesn't need to use the temp file we provide,
           so this test isn't entirely sound */
        fprintf(stderr, "ERROR: did not find %s (err=%d)\n",
            kTestTempFile, err);
        goto failed;
    }

    /*
     * Close it and confirm that the file has the expected name.
     */
    err = NuClose(pArchive);
    if (err != kNuErrNone) {
        fprintf(stderr, "ERROR: mid NuClose failed (err=%d)\n", err);
        goto failed;
    }
    pArchive = NULL;

    if (access(kTestArchive, F_OK) != 0) {
        fprintf(stderr, "ERROR: did not find %s (err=%d)\n", kTestArchive, err);
        goto failed;
    }

    /*
     * Reopen it read-only.
     */
    printf("... reopening archive read-only\n");
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
     * Extract the file.
     */
    if (TestExtract(pArchive, testEntryNameMOR) < 0) {
        goto failed;
    }

    /*
     * That's all, folks...
     */
    NuClose(pArchive);
    pArchive = NULL;

    printf("... removing '%s'\n", kTestArchive);
    if (unlink(kTestArchive) < 0) {
        perror("unlink kTestArchive");
        goto failed;
    }


leave:
    if (pArchive != NULL) {
        NuAbort(pArchive);
        NuClose(pArchive);
    }
    free(testEntryNameMOR);
    return result;

failed:
    result = -1;
    goto leave;
}


/*
 * Start here.
 */
int main(void)
{
    int32_t major, minor, bug;
    const char* pBuildDate;
    const char* pBuildFlags;
    int cc;

    (void) NuGetVersion(&major, &minor, &bug, &pBuildDate, &pBuildFlags);
    printf("Using NuFX library v%d.%d.%d, built on or after\n"
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

