/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Get a file from a disk image.
 */
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <assert.h>
#include "../diskimg/DiskImg.h"

using namespace DiskImgLib;

#define nil NULL
#define NELEM(x) (sizeof(x) / sizeof((x)[0]))

/*
 * Globals.
 */
FILE* gLog = nil;
pid_t gPid = getpid();

/*
 * Show usage info.
 */
void
Usage(const char* argv0)
{
    fprintf(stderr, "Usage: %s image-filename file\n", argv0);

    fprintf(stderr, "\n");
    fprintf(stderr, "The file will be written to stdout.\n");
}

/*
 * Copy a file from "src" to "dst".
 */
int
CopyFile(A2FileDescr* src, FILE* dst)
{
    DIError dierr;
    size_t actual;
    char buf[4096];

    while (1) {
        dierr = src->Read(buf, sizeof(buf), &actual);
        if (dierr != kDIErrNone) {
            fprintf(stderr, "Error: read failed: %s\n", DIStrError(dierr));
            return -1;
        }

        if (actual == 0)    // EOF hit
            break;

        fwrite(buf, 1, actual, dst);
    }

    return 0;
}

/*
 * Extract the named file from the specified image.
 */
int
Process(const char* imageName, const char* wantedFileName)
{
    DIError dierr;
    DiskImg diskImg;
    DiskFS* pDiskFS = nil;
    A2File* pFile = nil;
    A2FileDescr* pDescr = nil;
    int result = -1;

    /* open read-only */
    dierr = diskImg.OpenImage(imageName, '/', true);
    if (dierr != kDIErrNone) {
        fprintf(stderr, "Unable to open '%s': %s\n", imageName,
            DIStrError(dierr));
        goto bail;
    }

    /* figure out the format */
    dierr = diskImg.AnalyzeImage();
    if (dierr != kDIErrNone) {
        fprintf(stderr, "Analysis of '%s' failed: %s\n", imageName,
            DIStrError(dierr));
        goto bail;
    }

    /* recognized? */
    if (diskImg.GetFSFormat() == DiskImg::kFormatUnknown ||
        diskImg.GetSectorOrder() == DiskImg::kSectorOrderUnknown)
    {
        fprintf(stderr, "Unable to identify filesystem on '%s'\n", imageName);
        goto bail;
    }

    /* create an appropriate DiskFS object */
    pDiskFS = diskImg.OpenAppropriateDiskFS();
    if (pDiskFS == nil) {
        /* unknown FS should've been caught above! */
        assert(false);
        fprintf(stderr, "Format of '%s' not recognized.\n", imageName);
        goto bail;
    }

    /* go ahead and load up volumes mounted inside volumes */
    pDiskFS->SetScanForSubVolumes(DiskFS::kScanSubEnabled);

    /* do a full scan */
    dierr = pDiskFS->Initialize(&diskImg, DiskFS::kInitFull);
    if (dierr != kDIErrNone) {
        fprintf(stderr, "Error reading list of files from disk: %s\n",
            DIStrError(dierr));
        goto bail;
    }

    /*
     * Find the file.  This comes out of a list of entries, so don't
     * delete "pFile" when we're done.
     */
    pFile = pDiskFS->GetFileByName(wantedFileName);
    if (pFile == nil) {
        fprintf(stderr, "File '%s' not found in '%s'\n", wantedFileName,
            imageName);
        goto bail;
    }

    /*
     * Open the file read-only.
     */
    dierr = pFile->Open(&pDescr, true);
    if (dierr != kDIErrNone) {
        fprintf(stderr, "Error opening '%s': %s\n", wantedFileName,
            DIStrError(dierr));
        goto bail;
    }

    /*
     * Copy the file to stdout.
     */
    result = CopyFile(pDescr, stdout);

bail:
    if (pDescr != nil) {
        pDescr->Close();
        //delete pDescr;    -- don't do this (double free)
    }
    delete pDiskFS;
    return result;
}

/*
 * Handle a debug message from the DiskImg library.
 */
/*static*/ void
MsgHandler(const char* file, int line, const char* msg)
{
    assert(file != nil);
    assert(msg != nil);

#ifdef _DEBUG
    fprintf(gLog, "%05u %s", gPid, msg);
#endif
}

/*
 * Process args.
 */
int
main(int argc, char** argv)
{
#ifdef _DEBUG
    const char* kLogFile = "makedisk-log.txt";
    gLog = fopen(kLogFile, "w");
    if (gLog == nil) {
        fprintf(stderr, "ERROR: unable to open log file\n");
        exit(1);
    }
#endif

#ifdef _DEBUG
    fprintf(stderr, "Log file is '%s'\n", kLogFile);
#endif

    Global::SetDebugMsgHandler(MsgHandler);
    Global::AppInit();

    if (argc != 3) {
        Usage(argv[0]);
        exit(2);
    }

    const char* imageName;
    const char* getFileName;

    argv++;
    imageName = *argv++;
    getFileName = *argv++;
    argc -= 2;

    if (Process(imageName, getFileName) == 0)
        fprintf(stderr, "Success!\n");
    else
        fprintf(stderr, "Failed.\n");

    Global::AppCleanup();
#ifdef _DEBUG
    fclose(gLog);
#endif

    exit(0);
}

