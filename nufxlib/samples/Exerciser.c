/*
 * NuFX archive manipulation library
 * Copyright (C) 2000-2007 by Andy McFadden, All Rights Reserved.
 * This is free software; you can redistribute it and/or modify it under the
 * terms of the BSD License, see the file COPYING.LIB.
 *
 * NufxLib exerciser.  Most library functions can be invoked directly from
 * the exerciser command line.
 *
 * This was written in C++ to evaluate the interaction between NufxLib and
 * the C++ language, i.e. to make sure that all type definitions and
 * function calls can be used without giving the compiler fits.  This
 * file will compile as either "Exerciser.c" or "Exerciser.cpp".
 */
#include "NufxLib.h"
#include "Common.h"
#include <ctype.h>

/* not portable to other OSs, but not all that important anyway */
static const char kFssep = PATH_SEP;

/* ProDOS access permissions */
#define kUnlocked   0xe3

#define kTempFile   "exer-temp"


/*
 * ===========================================================================
 *      ExerciserState object
 * ===========================================================================
 */

/*
 * Exerciser state.
 *
 * In case it isn't immediately apparent, this was written in C++ and
 * then converted back to C.
 */
typedef struct ExerciserState {
    NuArchive* pArchive;
    char* archivePath;
    const char* archiveFile;
} ExerciserState;


ExerciserState* ExerciserState_New(void)
{
    ExerciserState* pExerState;
    
    pExerState = (ExerciserState*) malloc(sizeof(*pExerState));
    if (pExerState == NULL)
        return NULL;

    pExerState->pArchive = NULL;
    pExerState->archivePath = NULL;
    pExerState->archiveFile = NULL;

    return pExerState;
}

void ExerciserState_Free(ExerciserState* pExerState)
{
    if (pExerState == NULL)
        return;

    if (pExerState->pArchive != NULL) {
        printf("Exerciser: aborting open archive\n");
        (void) NuAbort(pExerState->pArchive);
        (void) NuClose(pExerState->pArchive);
    }
    if (pExerState->archivePath != NULL)
        free(pExerState->archivePath);

    free(pExerState);
}

inline NuArchive* ExerciserState_GetNuArchive(const ExerciserState* pExerState)
{
    return pExerState->pArchive;
}

inline void ExerciserState_SetNuArchive(ExerciserState* pExerState,
    NuArchive* newArchive)
{
    pExerState->pArchive = newArchive;
}

inline char* ExerciserState_GetArchivePath(const ExerciserState* pExerState)
{
    return pExerState->archivePath;
}

inline void ExerciserState_SetArchivePath(ExerciserState* pExerState,
    char* newPath)
{
    if (pExerState->archivePath != NULL)
        free(pExerState->archivePath);

    if (newPath == NULL) {
        pExerState->archivePath = NULL;
        pExerState->archiveFile = NULL;
    } else {
        pExerState->archivePath = strdup(newPath);
        pExerState->archiveFile = strrchr(newPath, kFssep);
        if (pExerState->archiveFile != NULL)
            pExerState->archiveFile++;

        if (pExerState->archiveFile == NULL || *pExerState->archiveFile == '\0')
            pExerState->archiveFile = pExerState->archivePath;
    }
}

inline const char* ExerciserState_GetArchiveFile(const ExerciserState* pExerState)
{
    if (pExerState->archiveFile == NULL)
        return "[no archive open]";
    else
        return pExerState->archiveFile;
}


/*
 * ===========================================================================
 *      Utility functions
 * ===========================================================================
 */

/*
 * NuContents callback function.  Print the contents of an individual record.
 */
NuResult PrintEntry(NuArchive* pArchive, void* vpRecord)
{
    const NuRecord* pRecord = (const NuRecord*) vpRecord;
    int idx;

    (void)pArchive; /* shut up, gcc */

    printf("RecordIdx %u: '%s'\n",
        pRecord->recordIdx, pRecord->filenameMOR);

    for (idx = 0; idx < (int) pRecord->recTotalThreads; idx++) {
        const NuThread* pThread;
        NuThreadID threadID;
        const char* threadLabel;

        pThread = NuGetThread(pRecord, idx);
        assert(pThread != NULL);

        threadID = NuGetThreadID(pThread);
        switch (NuThreadIDGetClass(threadID)) {
        case kNuThreadClassMessage:
            threadLabel = "message class";
            break;
        case kNuThreadClassControl:
            threadLabel = "control class";
            break;
        case kNuThreadClassData:
            threadLabel = "data class";
            break;
        case kNuThreadClassFilename:
            threadLabel = "filename class";
            break;
        default:
            threadLabel = "(unknown class)";
            break;
        }

        switch (threadID) {
        case kNuThreadIDComment:
            threadLabel = "comment";
            break;
        case kNuThreadIDIcon:
            threadLabel = "icon";
            break;
        case kNuThreadIDMkdir:
            threadLabel = "mkdir";
            break;
        case kNuThreadIDDataFork:
            threadLabel = "data fork";
            break;
        case kNuThreadIDDiskImage:
            threadLabel = "disk image";
            break;
        case kNuThreadIDRsrcFork:
            threadLabel = "rsrc fork";
            break;
        case kNuThreadIDFilename:
            threadLabel = "filename";
            break;
        default:
            break;
        }

        printf("  ThreadIdx %u - 0x%08x (%s)\n", pThread->threadIdx,
            threadID, threadLabel);
    }

    return kNuOK;
}


#define kNiceLineLen    256

/*
 * Get a line of input, stripping the '\n' off the end.
 */
static NuError GetLine(const char* prompt, char* buffer, int bufferSize)
{
    printf("%s> ", prompt);
    fflush(stdout);

    if (fgets(buffer, bufferSize, stdin) == NULL)
        return kNuErrGeneric;

    if (buffer[strlen(buffer)-1] == '\n')
        buffer[strlen(buffer)-1] = '\0';

    return kNuErrNone;
}


/*
 * Selection filter for mass "extract" and "delete" operations.
 */
NuResult SelectionFilter(NuArchive* pArchive, void* vselFilt)
{
    const NuSelectionProposal* selProposal = (NuSelectionProposal*) vselFilt;
    char buffer[8];

    printf("%s (N/y)? ", selProposal->pRecord->filenameMOR);
    fflush(stdout);

    if (fgets(buffer, sizeof(buffer), stdin) == NULL)
        return kNuAbort;

    if (tolower(buffer[0]) == 'y')
        return kNuOK;
    else
        return kNuSkip;
}


/*
 * General-purpose error handler.
 */
NuResult ErrorHandler(NuArchive* pArchive, void* vErrorStatus)
{
    const NuErrorStatus* pErrorStatus = (const NuErrorStatus*) vErrorStatus;
    char buffer[8];
    NuResult result = kNuSkip;

    printf("Exerciser: error handler op=%d err=%d sysErr=%d message='%s'\n"
            "\tfilename='%s' '%c'(0x%02x)\n",
        pErrorStatus->operation, pErrorStatus->err, pErrorStatus->sysErr,
        pErrorStatus->message == NULL ? "(NULL)" : pErrorStatus->message,
        pErrorStatus->pathnameUNI, pErrorStatus->filenameSeparator,
        pErrorStatus->filenameSeparator);
    printf("\tValid options are:");
    if (pErrorStatus->canAbort)
        printf(" a)bort");
    if (pErrorStatus->canRetry)
        printf(" r)etry");
    if (pErrorStatus->canIgnore)
        printf(" i)gnore");
    if (pErrorStatus->canSkip)
        printf(" s)kip");
    if (pErrorStatus->canRename)
        printf(" re)name");
    if (pErrorStatus->canOverwrite)
        printf(" o)verwrite");
    putc('\n', stdout);

    printf("Return what (a/r/i/s/e/o)? ");
    fflush(stdout);

    if (fgets(buffer, sizeof(buffer), stdin) == NULL) {
        printf("Returning kNuSkip\n");
    } else switch (buffer[0]) {
        case 'a':   result = kNuAbort;      break;
        case 'r':   result = kNuRetry;      break;
        case 'i':   result = kNuIgnore;     break;
        case 's':   result = kNuSkip;       break;
        case 'e':   result = kNuRename;     break;
        case 'o':   result = kNuOverwrite;  break;
        default:
            printf("Unknown value '%c', returning kNuSkip\n", buffer[0]);
            break;
    }

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
 * ===========================================================================
 *      Command handlers
 * ===========================================================================
 */

typedef NuError (*CommandFunc)(ExerciserState* pState, int argc,
    char** argv);

static NuError HelpFunc(ExerciserState* pState, int argc, char** argv);

#if 0
static NuError
GenericFunc(ExerciserState* pState, int argc, char** argv)
{
    (void) pState, (void) argc, (void) argv;    /* shut up, gcc */
    printf("Generic! argc=%d\n", argc);
    return kNuErrNone;
}
#endif

/*
 * Do nothing.  Useful when the user just hits <return> on a blank line.
 */
static NuError NothingFunc(ExerciserState* pState, int argc, char** argv)
{
    (void) pState, (void) argc, (void) argv;    /* shut up, gcc */
    return kNuErrNone;
}

/*
 * q - quit
 *
 * Do nothing.  This is used as a trigger for quitting the program.  In
 * practice, we catch this earlier, and won't actually call here.
 */
static NuError QuitFunc(ExerciserState* pState, int argc, char** argv)
{
    (void) pState, (void) argc, (void) argv;    /* shut up, gcc */
    assert(0);
    return kNuErrNone;
}



/*
 * ab - abort current changes
 */
static NuError AbortFunc(ExerciserState* pState, int argc, char** argv)
{
    (void) pState, (void) argc, (void) argv;    /* shut up, gcc */
    assert(ExerciserState_GetNuArchive(pState) != NULL);
    assert(argc == 1);

    return NuAbort(ExerciserState_GetNuArchive(pState));
}

/*
 * af - add file to archive
 */
static NuError AddFileFunc(ExerciserState* pState, int argc, char** argv)
{
    NuFileDetails nuFileDetails;

    (void) pState, (void) argc, (void) argv;    /* shut up, gcc */
    assert(ExerciserState_GetNuArchive(pState) != NULL);
    assert(argc == 2);

    memset(&nuFileDetails, 0, sizeof(nuFileDetails));
    nuFileDetails.threadID = kNuThreadIDDataFork;
    nuFileDetails.storageNameMOR = argv[1];
    nuFileDetails.fileSysID = kNuFileSysUnknown;
    nuFileDetails.fileSysInfo = (short) kFssep;
    nuFileDetails.access = kUnlocked;
    /* fileType, extraType, storageType, dates */

    return NuAddFile(ExerciserState_GetNuArchive(pState), argv[1],
            &nuFileDetails, false, NULL);
}

/*
 * ar - add an empty record
 */
static NuError AddRecordFunc(ExerciserState* pState, int argc, char** argv)
{
    NuError err;
    NuRecordIdx recordIdx;
    NuFileDetails nuFileDetails;

    (void) pState, (void) argc, (void) argv;    /* shut up, gcc */
    assert(ExerciserState_GetNuArchive(pState) != NULL);
    assert(argc == 2);

    memset(&nuFileDetails, 0, sizeof(nuFileDetails));
    nuFileDetails.threadID = 0;     /* irrelevant */
    nuFileDetails.storageNameMOR = argv[1];
    nuFileDetails.fileSysID = kNuFileSysUnknown;
    nuFileDetails.fileSysInfo = (short) kFssep;
    nuFileDetails.access = kUnlocked;
    /* fileType, extraType, storageType, dates */

    err = NuAddRecord(ExerciserState_GetNuArchive(pState),
            &nuFileDetails, &recordIdx);
    if (err == kNuErrNone)
        printf("Exerciser: success, new recordIdx=%u\n", recordIdx);
    return err;
}

/*
 * at - add thread to record
 */
static NuError AddThreadFunc(ExerciserState* pState, int argc, char** argv)
{
    NuError err;
    NuDataSource* pDataSource = NULL;
    char* lineBuf = NULL;
    long ourLen, maxLen;
    NuThreadID threadID;
    NuThreadIdx threadIdx;

    (void) pState, (void) argc, (void) argv;    /* shut up, gcc */
    assert(ExerciserState_GetNuArchive(pState) != NULL);
    assert(argc == 3);

    lineBuf = (char*)malloc(kNiceLineLen);
    assert(lineBuf != NULL);

    threadID = strtol(argv[2], NULL, 0);
    if (NuThreadIDGetClass(threadID) == kNuThreadClassData) {
        /* load data from a file on disk */
        maxLen = 0;
        err = GetLine("Enter filename", lineBuf, kNiceLineLen);
        if (err != kNuErrNone)
            goto bail;
        if (!lineBuf[0]) {
            fprintf(stderr, "Invalid filename\n");
            err = kNuErrInvalidArg;
            goto bail;
        }

        err = NuCreateDataSourceForFile(kNuThreadFormatUncompressed,
                0, lineBuf, false, &pDataSource);
        if (err != kNuErrNone) {
            fprintf(stderr,
                "Exerciser: file data source create failed (err=%d)\n", err);
            goto bail;
        }
    } else {
        if (threadID == kNuThreadIDFilename || threadID == kNuThreadIDComment) {
            /* select the buffer pre-size */
            err = GetLine("Enter max buffer size", lineBuf, kNiceLineLen);
            if (err != kNuErrNone)
                goto bail;
            maxLen = strtol(lineBuf, NULL, 0);
            if (maxLen <= 0) {
                fprintf(stderr, "Bad length\n");
                err = kNuErrInvalidArg;
                goto bail;
            }
        } else {
            maxLen = 0;
        }

        err = GetLine("Enter the thread contents", lineBuf, kNiceLineLen);
        if (err != kNuErrNone)
            goto bail;
        ourLen = strlen(lineBuf);

        /* create a data source from the buffer */
        err = NuCreateDataSourceForBuffer(kNuThreadFormatUncompressed,
                maxLen, (uint8_t*)lineBuf, 0, ourLen, FreeCallback,
                &pDataSource);
        if (err != kNuErrNone) {
            fprintf(stderr,
                "Exerciser: buffer data source create failed (err=%d)\n", err);
            goto bail;
        }
        lineBuf = NULL;  /* now owned by the library */
    }


    err = NuAddThread(ExerciserState_GetNuArchive(pState),
            strtol(argv[1], NULL, 0), threadID, pDataSource, &threadIdx);
    if (err == kNuErrNone) {
        pDataSource = NULL;  /* library owns it now */
        printf("Exerciser: success; function returned threadIdx=%u\n",
            threadIdx);
    }

bail:
    NuFreeDataSource(pDataSource);
    if (lineBuf != NULL)
        free(lineBuf);
    return err;
}

/*
 * cl - close archive
 */
static NuError CloseFunc(ExerciserState* pState, int argc, char** argv)
{
    NuError err;

    (void) pState, (void) argc, (void) argv;    /* shut up, gcc */
    assert(ExerciserState_GetNuArchive(pState) != NULL);
    assert(argc == 1);

    err = NuClose(ExerciserState_GetNuArchive(pState));
    if (err == kNuErrNone) {
        ExerciserState_SetNuArchive(pState, NULL);
        ExerciserState_SetArchivePath(pState, NULL);
    }

    return err;
}

/*
 * d - delete all records (selection-filtered)
 */
static NuError DeleteFunc(ExerciserState* pState, int argc, char** argv)
{
    (void) pState, (void) argc, (void) argv;    /* shut up, gcc */
    assert(ExerciserState_GetNuArchive(pState) != NULL);
    assert(argc == 1);

    NuSetSelectionFilter(ExerciserState_GetNuArchive(pState), SelectionFilter);

    return NuDelete(ExerciserState_GetNuArchive(pState));
}

/*
 * dr - delete record
 */
static NuError DeleteRecordFunc(ExerciserState* pState, int argc, char** argv)
{
    (void) pState, (void) argc, (void) argv;    /* shut up, gcc */
    assert(ExerciserState_GetNuArchive(pState) != NULL);
    assert(argc == 2);

    return NuDeleteRecord(ExerciserState_GetNuArchive(pState),
            strtol(argv[1], NULL, 0));
}

/*
 * dt - delete thread
 */
static NuError DeleteThreadFunc(ExerciserState* pState, int argc, char** argv)
{
    (void) pState, (void) argc, (void) argv;    /* shut up, gcc */
    assert(ExerciserState_GetNuArchive(pState) != NULL);
    assert(argc == 2);

    return NuDeleteThread(ExerciserState_GetNuArchive(pState),
            strtol(argv[1], NULL, 0));
}

/*
 * e - extract all files (selection-filtered)
 */
static NuError ExtractFunc(ExerciserState* pState, int argc, char** argv)
{
    (void) pState, (void) argc, (void) argv;    /* shut up, gcc */
    assert(ExerciserState_GetNuArchive(pState) != NULL);
    assert(argc == 1);

    NuSetSelectionFilter(ExerciserState_GetNuArchive(pState), SelectionFilter);

    return NuExtract(ExerciserState_GetNuArchive(pState));
}

/*
 * er - extract record
 */
static NuError ExtractRecordFunc(ExerciserState* pState, int argc, char** argv)
{
    (void) pState, (void) argc, (void) argv;    /* shut up, gcc */
    assert(ExerciserState_GetNuArchive(pState) != NULL);
    assert(argc == 2);

    return NuExtractRecord(ExerciserState_GetNuArchive(pState),
                strtol(argv[1], NULL, 0));
}

/*
 * et - extract thread
 */
static NuError ExtractThreadFunc(ExerciserState* pState, int argc, char** argv)
{
    NuError err;
    NuDataSink* pDataSink = NULL;

    (void) pState, (void) argc, (void) argv;    /* shut up, gcc */
    assert(ExerciserState_GetNuArchive(pState) != NULL);
    assert(argc == 3);

    err = NuCreateDataSinkForFile(true, kNuConvertOff, argv[2], kFssep,
            &pDataSink);
    if (err != kNuErrNone) {
        fprintf(stderr, "Exerciser: data sink create failed\n");
        goto bail;
    }

    err = NuExtractThread(ExerciserState_GetNuArchive(pState),
            strtol(argv[1], NULL, 0), pDataSink);
    /* fall through with err */

bail:
    NuFreeDataSink(pDataSink);
    return err;
}

/*
 * fl - flush changes to archive
 */
static NuError FlushFunc(ExerciserState* pState, int argc, char** argv)
{
    NuError err;
    uint32_t flushStatus;

    (void) pState, (void) argc, (void) argv;    /* shut up, gcc */
    assert(ExerciserState_GetNuArchive(pState) != NULL);
    assert(argc == 1);

    err = NuFlush(ExerciserState_GetNuArchive(pState), &flushStatus);
    if (err != kNuErrNone)
        printf("Exerciser: flush failed, status flags=0x%04x\n", flushStatus);
    return err;
}

/*
 * gev - get value
 *
 * Currently takes numeric arguments.  We could be nice and accept the
 * things like "IgnoreCRC" for kNuValueIgnoreCRC, but not yet.
 */
static NuError GetValueFunc(ExerciserState* pState, int argc, char** argv)
{
    NuError err;
    NuValue value;

    (void) pState, (void) argc, (void) argv;    /* shut up, gcc */
    assert(ExerciserState_GetNuArchive(pState) != NULL);
    assert(argc == 2);

    err = NuGetValue(ExerciserState_GetNuArchive(pState),
            (NuValueID) strtol(argv[1], NULL, 0), &value);
    if (err == kNuErrNone)
        printf("  --> %u\n", value);
    return err;
}

/*
 * gmh - get master header
 */
static NuError GetMasterHeaderFunc(ExerciserState* pState, int argc,
    char** argv)
{
    NuError err;
    const NuMasterHeader* pMasterHeader;

    (void) pState, (void) argc, (void) argv;    /* shut up, gcc */
    assert(ExerciserState_GetNuArchive(pState) != NULL);
    assert(argc == 1);

    err = NuGetMasterHeader(ExerciserState_GetNuArchive(pState),
            &pMasterHeader);
    if (err == kNuErrNone) {
        printf("Exerciser: success (version=%u, totalRecords=%u, EOF=%u)\n",
            pMasterHeader->mhMasterVersion, pMasterHeader->mhTotalRecords,
            pMasterHeader->mhMasterEOF);
    }
    return err;
}

/*
 * gr - get record attributes
 */
static NuError GetRecordFunc(ExerciserState* pState, int argc, char** argv)
{
    NuError err;
    const NuRecord* pRecord;

    (void) pState, (void) argc, (void) argv;    /* shut up, gcc */
    assert(ExerciserState_GetNuArchive(pState) != NULL);
    assert(argc == 2);

    err = NuGetRecord(ExerciserState_GetNuArchive(pState),
            strtol(argv[1], NULL, 0), &pRecord);
    if (err == kNuErrNone) {
        printf("Exerciser: success, call returned:\n");
        printf("\tfileSysID   : %d\n", pRecord->recFileSysID);
        printf("\tfileSysInfo : 0x%04x ('%c')\n", pRecord->recFileSysInfo,
            NuGetSepFromSysInfo(pRecord->recFileSysInfo));
        printf("\taccess      : 0x%02x\n", pRecord->recAccess);
        printf("\tfileType    : 0x%04x\n", pRecord->recFileType);
        printf("\textraType   : 0x%04x\n", pRecord->recExtraType);
        printf("\tcreateWhen  : ...\n");
        printf("\tmodWhen     : ...\n");        /* too lazy */
        printf("\tarchiveWhen : ...\n");
    }
    return err;
}

/*
 * grin - get record idx by name
 */
static NuError GetRecordIdxByNameFunc(ExerciserState* pState, int argc,
    char** argv)
{
    NuError err;
    NuRecordIdx recIdx;

    (void) pState, (void) argc, (void) argv;    /* shut up, gcc */
    assert(ExerciserState_GetNuArchive(pState) != NULL);
    assert(argc == 2);

    err = NuGetRecordIdxByName(ExerciserState_GetNuArchive(pState),
            argv[1], &recIdx);
    if (err == kNuErrNone)
        printf("Exerciser: success, returned recordIdx=%u\n", recIdx);
    return err;
}

/*
 * grip - get record idx by position
 */
static NuError GetRecordIdxByPositionFunc(ExerciserState* pState, int argc,
    char** argv)
{
    NuError err;
    NuRecordIdx recIdx;

    (void) pState, (void) argc, (void) argv;    /* shut up, gcc */
    assert(ExerciserState_GetNuArchive(pState) != NULL);
    assert(argc == 2);

    err = NuGetRecordIdxByPosition(ExerciserState_GetNuArchive(pState),
            strtol(argv[1], NULL, 0), &recIdx);
    if (err == kNuErrNone)
        printf("Exerciser: success, returned recordIdx=%u\n", recIdx);
    return err;
}

/*
 * ocrw - open/create read-write
 */
static NuError OpenCreateReadWriteFunc(ExerciserState* pState, int argc,
    char** argv)
{
    NuError err;
    NuArchive* pArchive;

    assert(ExerciserState_GetNuArchive(pState) == NULL);
    assert(argc == 2);

    err = NuOpenRW(argv[1], kTempFile, kNuOpenCreat|kNuOpenExcl, &pArchive);
    if (err == kNuErrNone) {
        ExerciserState_SetNuArchive(pState, pArchive);
        ExerciserState_SetArchivePath(pState, argv[1]);
    }

    return err;
}

/*
 * oro - open read-only
 */
static NuError OpenReadOnlyFunc(ExerciserState* pState, int argc, char** argv)
{
    NuError err;
    NuArchive* pArchive;

    assert(ExerciserState_GetNuArchive(pState) == NULL);
    assert(argc == 2);

    err = NuOpenRO(argv[1], &pArchive);
    if (err == kNuErrNone) {
        ExerciserState_SetNuArchive(pState, pArchive);
        ExerciserState_SetArchivePath(pState, argv[1]);
    }

    return err;
}

/*
 * ors - open streaming read-only
 */
static NuError OpenStreamingReadOnlyFunc(ExerciserState* pState, int argc,
    char** argv)
{
    NuError err;
    NuArchive* pArchive;
    FILE* fp = NULL;

    assert(ExerciserState_GetNuArchive(pState) == NULL);
    assert(argc == 2);

    if ((fp = fopen(argv[1], kNuFileOpenReadOnly)) == NULL) {
        err = errno ? (NuError)errno : kNuErrGeneric;
        fprintf(stderr, "Exerciser: unable to open '%s'\n", argv[1]);
    } else {
        err = NuStreamOpenRO(fp, &pArchive);
        if (err == kNuErrNone) {
            ExerciserState_SetNuArchive(pState, pArchive);
            ExerciserState_SetArchivePath(pState, argv[1]);
            fp = NULL;
        }
    }

    if (fp != NULL)
        fclose(fp);

    return err;
}

/*
 * orw - open read-write
 */
static NuError OpenReadWriteFunc(ExerciserState* pState, int argc, char** argv)
{
    NuError err;
    NuArchive* pArchive;

    assert(ExerciserState_GetNuArchive(pState) == NULL);
    assert(argc == 2);

    err = NuOpenRW(argv[1], kTempFile, 0, &pArchive);
    if (err == kNuErrNone) {
        ExerciserState_SetNuArchive(pState, pArchive);
        ExerciserState_SetArchivePath(pState, argv[1]);
    }

    return err;
}

/*
 * p - print
 */
static NuError PrintFunc(ExerciserState* pState, int argc, char** argv)
{
    (void) pState, (void) argc, (void) argv;    /* shut up, gcc */
    assert(ExerciserState_GetNuArchive(pState) != NULL);
    assert(argc == 1);

    return NuContents(ExerciserState_GetNuArchive(pState), PrintEntry);
}

/*
 * pd - print debug
 */
static NuError PrintDebugFunc(ExerciserState* pState, int argc, char** argv)
{
    (void) pState, (void) argc, (void) argv;    /* shut up, gcc */
    assert(ExerciserState_GetNuArchive(pState) != NULL);
    assert(argc == 1);

    return NuDebugDumpArchive(ExerciserState_GetNuArchive(pState));
}

/*
 * re - rename record
 */
static NuError RenameFunc(ExerciserState* pState, int argc, char** argv)
{
    (void) pState, (void) argc, (void) argv;    /* shut up, gcc */
    assert(ExerciserState_GetNuArchive(pState) != NULL);
    assert(argc == 4);

    return NuRename(ExerciserState_GetNuArchive(pState),
            strtol(argv[1], NULL, 0), argv[2], argv[3][0]);
}

/*
 * sec - set error callback
 *
 * Use an error handler callback.
 */
static NuError SetErrorCallbackFunc(ExerciserState* pState, int argc,
    char** argv)
{
    (void) pState, (void) argc, (void) argv;    /* shut up, gcc */
    assert(ExerciserState_GetNuArchive(pState) != NULL);
    assert(argc == 1);

    NuSetErrorHandler(ExerciserState_GetNuArchive(pState), ErrorHandler);
    return kNuErrNone;
}

/*
 * sev - set value
 *
 * Currently takes numeric arguments.
 */
static NuError SetValueFunc(ExerciserState* pState, int argc, char** argv)
{
    (void) pState, (void) argc, (void) argv;    /* shut up, gcc */
    assert(ExerciserState_GetNuArchive(pState) != NULL);
    assert(argc == 3);

    return NuSetValue(ExerciserState_GetNuArchive(pState),
            (NuValueID) strtol(argv[1], NULL, 0), strtol(argv[2], NULL, 0));
}

/*
 * sra - set record attributes
 *
 * Right now I'm only allowing changes to file type and aux type.  This
 * could be adapted to do more easily, but the command handler has a
 * rigid notion of how many arguments each function should have, so
 * you'd need to list all of them every time.
 */
static NuError SetRecordAttrFunc(ExerciserState* pState, int argc, char** argv)
{
    NuError err;
    const NuRecord* pRecord;
    NuRecordAttr recordAttr;

    (void) pState, (void) argc, (void) argv;    /* shut up, gcc */
    assert(ExerciserState_GetNuArchive(pState) != NULL);
    assert(argc == 4);

    err = NuGetRecord(ExerciserState_GetNuArchive(pState),
            strtol(argv[1], NULL, 0), &pRecord);
    if (err != kNuErrNone)
        return err;
    printf("Exerciser: NuGetRecord succeeded, calling NuSetRecordAttr\n");
    NuRecordCopyAttr(&recordAttr, pRecord);
    recordAttr.fileType = strtol(argv[2], NULL, 0);
    recordAttr.extraType = strtol(argv[3], NULL, 0);
    /*recordAttr.fileSysInfo = ':';*/
    return NuSetRecordAttr(ExerciserState_GetNuArchive(pState),
            strtol(argv[1], NULL, 0), &recordAttr);
}

/*
 * t - test archive
 */
static NuError TestFunc(ExerciserState* pState, int argc, char** argv)
{
    (void) pState, (void) argc, (void) argv;    /* shut up, gcc */
    assert(ExerciserState_GetNuArchive(pState) != NULL);
    assert(argc == 1);

    return NuTest(ExerciserState_GetNuArchive(pState));
}

/*
 * tr - test record
 */
static NuError TestRecordFunc(ExerciserState* pState, int argc, char** argv)
{
    (void) pState, (void) argc, (void) argv;    /* shut up, gcc */
    assert(ExerciserState_GetNuArchive(pState) != NULL);
    assert(argc == 2);

    return NuTestRecord(ExerciserState_GetNuArchive(pState),
            strtol(argv[1], NULL, 0));
}

/*
 * upt - update pre-sized thread
 */
static NuError UpdatePresizedThreadFunc(ExerciserState* pState, int argc,
    char** argv)
{
    NuError err;
    NuDataSource* pDataSource = NULL;
    char* lineBuf = NULL;
    long ourLen;
    int32_t maxLen;

    (void) pState, (void) argc, (void) argv;    /* shut up, gcc */
    assert(ExerciserState_GetNuArchive(pState) != NULL);
    assert(argc == 2);

    lineBuf = (char*)malloc(kNiceLineLen);
    assert(lineBuf != NULL);
    err = GetLine("Enter data for thread", lineBuf, kNiceLineLen);
    if (err != kNuErrNone)
        goto bail;

    ourLen = strlen(lineBuf);

    /* use "ourLen" for both buffer len and data len */
    err = NuCreateDataSourceForBuffer(kNuThreadFormatUncompressed,
            ourLen, (uint8_t*)lineBuf, 0, ourLen, FreeCallback,
            &pDataSource);
    if (err != kNuErrNone) {
        fprintf(stderr, "Exerciser: data source create failed (err=%d)\n",
            err);
        goto bail;
    }
    lineBuf = NULL;  /* now owned by the library */

    err = NuUpdatePresizedThread(ExerciserState_GetNuArchive(pState),
            strtol(argv[1], NULL, 0), pDataSource, &maxLen);
    if (err == kNuErrNone)
        printf("Exerciser: success; function returned maxLen=%d\n", maxLen);

bail:
    NuFreeDataSource(pDataSource);
    if (lineBuf != NULL)
        free(lineBuf);
    return err;
}


/*
 * Command table.  This drives the user interface.
 */

/* flags for the CommandTable */
#define kFlagArchiveReq         (1L)        /* must have archive open */
#define kFlagNoArchiveReq       (1L<<1)     /* must NOT have archive open */

/* command set */
static const struct {
    const char*     commandStr;
    CommandFunc     func;
    int             expectedArgCount;
    const char*     argumentList;
    uint32_t        flags;
    const char*     description;
} gCommandTable[] = {
    { "--- exerciser commands ---", HelpFunc, 0, "", 0,
        "" },
    { "?", HelpFunc, 0, "", 0,
        "Show help" },
    { "h", HelpFunc, 0, "", 0,
        "Show help" },
    { "q", QuitFunc, 0, "", 0,
        "Quit program (will abort un-flushed changes)" },

    { "--- archive commands ---", HelpFunc, 0, "", 0,
        "" },

    { "ab", AbortFunc, 0, "", kFlagArchiveReq,
        "Abort current changes" },
    { "af", AddFileFunc, 1, "filename", kFlagArchiveReq,
        "Add file" },
    { "ar", AddRecordFunc, 1, "storageName", kFlagArchiveReq,
        "Add record" },
    { "at", AddThreadFunc, 2, "recordIdx threadID", kFlagArchiveReq,
        "Add thread to record" },
    { "cl", CloseFunc, 0, "", kFlagArchiveReq,
        "Close archive after flushing any changes" },
    { "d", DeleteFunc, 0, "", kFlagArchiveReq,
        "Delete all records" },
    { "dr", DeleteRecordFunc, 1, "recordIdx", kFlagArchiveReq,
        "Delete record" },
    { "dt", DeleteThreadFunc, 1, "threadIdx", kFlagArchiveReq,
        "Delete thread" },
    { "e", ExtractFunc, 0, "", kFlagArchiveReq,
        "Extract all files" },
    { "er", ExtractRecordFunc, 1, "recordIdx", kFlagArchiveReq,
        "Extract record" },
    { "et", ExtractThreadFunc, 2, "threadIdx filename", kFlagArchiveReq,
        "Extract thread" },
    { "fl", FlushFunc, 0, "", kFlagArchiveReq,
        "Flush changes" },
    { "gev", GetValueFunc, 1, "ident", kFlagArchiveReq,
        "Get value" },
    { "gmh", GetMasterHeaderFunc, 0, "", kFlagArchiveReq,
        "Get master header" },
    { "gr", GetRecordFunc, 1, "recordIdx", kFlagArchiveReq,
        "Get record" },
    { "grin", GetRecordIdxByNameFunc, 1, "name", kFlagArchiveReq,
        "Get recordIdx by name" },
    { "grip", GetRecordIdxByPositionFunc, 1, "position", kFlagArchiveReq,
        "Get recordIdx by position" },
    { "ocrw", OpenCreateReadWriteFunc, 1, "filename", kFlagNoArchiveReq,
        "Open/create archive read-write" },
    { "oro", OpenReadOnlyFunc, 1, "filename", kFlagNoArchiveReq,
        "Open archive read-only" },
    { "ors", OpenStreamingReadOnlyFunc, 1, "filename", kFlagNoArchiveReq,
        "Open archive streaming read-only" },
    { "orw", OpenReadWriteFunc, 1, "filename", kFlagNoArchiveReq,
        "Open archive read-write" },
    { "p", PrintFunc, 0, "", kFlagArchiveReq,
        "Print archive contents" },
    { "pd", PrintDebugFunc, 0, "", kFlagArchiveReq,
        "Print debugging output (if available)" },
    { "re", RenameFunc, 3, "recordIdx name sep", kFlagArchiveReq,
        "Rename record" },
    { "sec", SetErrorCallbackFunc, 0, "", kFlagArchiveReq,
        "Set error callback" },
    { "sev", SetValueFunc, 2, "ident value", kFlagArchiveReq,
        "Set value" },
    { "sra", SetRecordAttrFunc, 3, "recordIdx type aux", kFlagArchiveReq,
        "Set record attributes" },
    { "t", TestFunc, 0, "", kFlagArchiveReq,
        "Test archive" },
    { "tr", TestRecordFunc, 1, "recordIdx", kFlagArchiveReq,
        "Test record" },
    { "upt", UpdatePresizedThreadFunc, 1, "threadIdx", kFlagArchiveReq,
        "Update pre-sized thread" },
};

#define kMaxArgs    4

/*
 * Display a summary of available commands.
 */
static NuError HelpFunc(ExerciserState* pState, int argc, char** argv)
{
    int i;

    (void) pState, (void) argc, (void) argv;    /* shut up, gcc */

    printf("\nAvailable commands:\n");
    for (i = 0; i < (int)NELEM(gCommandTable); i++) {
        printf("  %-4s %-21s %s\n",
            gCommandTable[i].commandStr,
            gCommandTable[i].argumentList,
            gCommandTable[i].description);
    }

    return kNuErrNone;
}


/*
 * ===========================================================================
 *      Control
 * ===========================================================================
 */

static const char* kWhitespace = " \t\n";

/*
 * Parse a command from the user.
 *
 * "lineBuf" will be mangled.  On success, "pFunc", "pArgc", and "pArgv"
 * will receive the results.
 */
static NuError ParseLine(char* lineBuf, ExerciserState* pState,
    CommandFunc* pFunc, int* pArgc, char*** pArgv)
{
    NuError err = kNuErrSyntax;
    char* command;
    char* cp;
    int i;

    /*
     * Parse the strings.
     */

    command = strtok(lineBuf, kWhitespace);
    if (command == NULL) {
        /* no command; the user probably just hit "enter" on a blank line */
        *pFunc = NothingFunc;
        *pArgc = 0;
        *pArgv = NULL;
        err = kNuErrNone;
        goto bail;
    }

    /* no real need to be flexible; add 1 for command and one for NULL */
    *pArgv = (char**) malloc(sizeof(char*) * (kMaxArgs+2));
    (*pArgv)[0] = command;
    *pArgc = 1;

    cp = strtok(NULL, kWhitespace);
    while (cp != NULL) {
        if (*pArgc >= kMaxArgs+1) {
            printf("ERROR: too many arguments\n");
            goto bail;
        }
        (*pArgv)[*pArgc] = cp;
        (*pArgc)++;

        cp = strtok(NULL, kWhitespace);
    }
    assert(*pArgc < kMaxArgs+2);
    (*pArgv)[*pArgc] = NULL;

    /*
     * Look up the command.
     */
    for (i = 0; i < (int)NELEM(gCommandTable); i++) {
        if (strcmp(command, gCommandTable[i].commandStr) == 0)
            break;
    }
    if (i == NELEM(gCommandTable)) {
        printf("ERROR: unrecognized command\n");
        goto bail;
    }

    *pFunc = gCommandTable[i].func;

    /*
     * Check arguments and flags.
     */
    if (*pArgc -1 != gCommandTable[i].expectedArgCount) {
        printf("ERROR: expected %d args, found %d\n",
            gCommandTable[i].expectedArgCount, *pArgc -1);
        goto bail;
    }

    if (gCommandTable[i].flags & kFlagArchiveReq) {
        if (ExerciserState_GetNuArchive(pState) == NULL) {
            printf("ERROR: must have an archive open\n");
            goto bail;
        }
    }
    if (gCommandTable[i].flags & kFlagNoArchiveReq) {
        if (ExerciserState_GetNuArchive(pState) != NULL) {
            printf("ERROR: an archive is already open\n");
            goto bail;
        }
    }

    /*
     * Looks good!
     */
    err = kNuErrNone;

bail:
    return err;
}


/*
 * Interpret commands, do clever things.
 */
static NuError CommandLoop(void)
{
    NuError err = kNuErrNone;
    ExerciserState* pState = ExerciserState_New();
    CommandFunc func;
    char lineBuf[128];
    int argc;
    char** argv = NULL;

    while (1) {
        printf("\nEnter command (%s)> ", ExerciserState_GetArchiveFile(pState));
        fflush(stdout);

        if (fgets(lineBuf, sizeof(lineBuf), stdin) == NULL) {
            printf("\n");
            break;
        }

        if (argv != NULL) {
            free(argv);
            argv = NULL;
        }

        func = NULL; /* sanity check */

        err = ParseLine(lineBuf, pState, &func, &argc, &argv);
        if (err != kNuErrNone)
            continue;

        assert(func != NULL);
        if (func == QuitFunc)
            break;

        err = (*func)(pState, argc, argv);

        if (err < 0)
            printf("Exerciser: received error %d (%s)\n", err, NuStrError(err));
        else if (err > 0)
            printf("Exerciser: received error %d\n", err);

        if (argv != NULL) {
            free(argv);
            argv = NULL;
        }
    }

    if (ExerciserState_GetNuArchive(pState) != NULL) {
        /* ought to query the archive before saying something like this... */
        printf("Exerciser: aborting any un-flushed changes in archive %s\n",
            ExerciserState_GetArchivePath(pState));
        (void) NuAbort(ExerciserState_GetNuArchive(pState));
        err = NuClose(ExerciserState_GetNuArchive(pState));
        if (err != kNuErrNone)
            printf("Exerciser: got error %d closing archive\n", err);
        ExerciserState_SetNuArchive(pState, NULL);
    }

    if (pState != NULL)
        ExerciserState_Free(pState);
    if (argv != NULL)
        free(argv);
    return kNuErrNone;
}


/*
 * Main entry point.
 *
 * We don't currently take any arguments, so this is pretty straightforward.
 */
int main(void)
{
    NuError result;
    int32_t majorVersion, minorVersion, bugVersion;
    const char* nufxLibDate;
    const char* nufxLibFlags;

    (void) NuGetVersion(&majorVersion, &minorVersion, &bugVersion,
            &nufxLibDate, &nufxLibFlags);
    printf("NufxLib exerciser, linked with NufxLib v%d.%d.%d [%s]\n\n",
        majorVersion, minorVersion, bugVersion, nufxLibFlags);
    printf("Use 'h' or '?' for help, 'q' to quit.\n");

    /* stuff useful when debugging lots */
    if (unlink(kTempFile) == 0)
        fprintf(stderr, "NOTE: whacked exer-temp\n");
    if (unlink("new.shk") == 0)
        fprintf(stderr, "NOTE: whacked new.shk\n");

#if defined(HAS_MALLOC_CHECK_) && !defined(USE_DMALLOC)
    /*
     * This is really nice to have on Linux and any other system that
     * uses the GNU libc/malloc stuff.  It slows things down, but it
     * tells you when you do something dumb with malloc/realloc/free.
     * (Solaris 2.7 has a similar feature that is enabled by setting the
     * environment variable LD_PRELOAD to include watchmalloc.so.  Other
     * OSs and 3rd-party malloc packages may have similar features.)
     *
     * This environment variable must be set when the program is launched.
     * Tweaking the environment within the program has no effect.
     *
     * Now that the Linux world has "valgrind", this is probably
     * unnecessary.
     */
    {
        char* debugSet = getenv("MALLOC_CHECK_");
        if (debugSet == NULL)
            printf("WARNING: MALLOC_CHECK_ not enabled\n\n");
    }
#endif

    result = CommandLoop();

    exit(result != kNuErrNone);
}

