/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Create a blank disk image, format it, and copy some files onto it.
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
    fprintf(stderr,
        "Usage: %s {dos|prodos|pascal} size image-filename.po input-file1 ...\n",
        argv0);

    fprintf(stderr, "\n");
    fprintf(stderr, "Example: makedisk prodos 800k foo.po file1.txt file2.txt\n");
}


/*
 * Create a ProDOS-ordered disk image.
 *
 * Returns a DiskImg pointer on success, or nil on failure.
 */
DiskImg*
CreateDisk(const char* fileName, long blockCount)
{
    DIError dierr;
    DiskImg* pDiskImg = nil;

    pDiskImg = new DiskImg;
    dierr = pDiskImg->CreateImage(
        fileName,
        nil,                            // storageName
        DiskImg::kOuterFormatNone,
        DiskImg::kFileFormatUnadorned,
        DiskImg::kPhysicalFormatSectors,
        nil,                            // pNibbleDescr
        DiskImg::kSectorOrderProDOS,
        DiskImg::kFormatGenericProDOSOrd,
        blockCount,
        true);                          // no need to format the image

    if (dierr != kDIErrNone) {
        fprintf(stderr, "ERROR: CreateImage failed: %s\n",
            DIStrError(dierr));
        delete pDiskImg;
        pDiskImg = nil;
    }

    return pDiskImg;
}

/*
 * Copy files to the disk.
 */
int
CopyFiles(DiskFS* pDiskFS, int argc, char** argv)
{
    DIError dierr;
    DiskFS::CreateParms parms;
    A2File* pNewFile;

    struct CreateParms {
        const char*     pathName;       // full pathname
        char            fssep;
        int             storageType;    // determines normal, subdir, or forked
        long            fileType;
        long            auxType;
        int             access;
        time_t          createWhen;
        time_t          modWhen;
    };


    while (argc--) {
        printf("+++ Adding '%s'\n", *argv);

        /*
         * Use external pathname as internal pathname.  This isn't quite
         * right, since things like "../" will end up getting converted
         * to something we don't want, but it'll do for now.
         */
        parms.pathName = *argv;
        parms.fssep = '/';      // UNIX fssep
        parms.storageType = DiskFS::kStorageSeedling;   // not forked, not dir
        parms.fileType = 0;     // NON
        parms.auxType = 0;      // $0000
        parms.access = DiskFS::kFileAccessUnlocked;
        parms.createWhen = time(nil);
        parms.modWhen = time(nil);

        /*
         * Create a new, empty file.  The "pNewFile" pointer does not belong
         * to us, so we should not delete it later, or try to access it
         * after the underlying file is deleted.
         */
        dierr = pDiskFS->CreateFile(&parms, &pNewFile);
        if (dierr != kDIErrNone) {
            fprintf(stderr, "ERROR: unable to create '%s': %s\n",
                *argv, DIStrError(dierr));
            return -1;
        }

        /*
         * Load the input file into memory.
         */
        FILE* fp;
        char* buf;
        long len;

        fp = fopen(*argv, "r");
        if (fp == nil) {
            fprintf(stderr, "ERROR: unable to open input file '%s': %s\n",
                *argv, strerror(errno));
            return -1;
        }

        if (fseek(fp, 0, SEEK_END) != 0) {
            fprintf(stderr, "ERROR: unable to seek input file '%s': %s\n",
                *argv, strerror(errno));
            fclose(fp);
            return -1;
        }

        len = ftell(fp);
        rewind(fp);

        buf = new char[len];
        if (buf == nil) {
            fprintf(stderr, "ERROR: unable to alloc %ld bytes\n", len);
            fclose(fp);
            return -1;
        }

        if (fread(buf, len, 1, fp) != 1) {
            fprintf(stderr, "ERROR: fread of %ld bytes from '%s' failed: %s\n",
                len, *argv, strerror(errno));
            fclose(fp);
            delete[] buf;
            return -1;
        }
        fclose(fp);

        /*
         * Write the buffer to the disk image.
         *
         * The A2FileDescr object is created by "Open" and deleted by
         * "Close".
         */
        A2FileDescr* pFD;

        dierr = pNewFile->Open(&pFD, true);
        if (dierr != kDIErrNone) {
            fprintf(stderr, "ERROR: unable to open new file '%s': %s\n",
                pNewFile->GetPathName(), DIStrError(dierr));
            delete[] buf;
            return -1;
        }

        dierr = pFD->Write(buf, len);
        if (dierr != kDIErrNone) {
            fprintf(stderr, "ERROR: failed writing to '%s': %s\n",
                pNewFile->GetPathName(), DIStrError(dierr));
            pFD->Close();
            pDiskFS->DeleteFile(pNewFile);
            delete[] buf;
            return -1;
        }
        delete[] buf;

        dierr = pFD->Close();
        if (dierr != kDIErrNone) {
            fprintf(stderr, "ERROR: failed while closing '%s': %s\n",
                pNewFile->GetPathName(), DIStrError(dierr));
            return -1;
        }

        /*
         * On to the next file.
         */
        argv++;
    }

    return 0;
}

/*
 * Process the request.
 *
 * Returns 0 on success, -1 on failure.
 */
int
Process(const char* formatName, const char* sizeStr,
    const char* outputFileName, int argc, char** argv)
{
    DiskImg::FSFormat format;
    long blockCount;

    if (strcasecmp(formatName, "dos") == 0)
        format = DiskImg::kFormatDOS33;
    else if (strcasecmp(formatName, "prodos") == 0)
        format = DiskImg::kFormatProDOS;
    else if (strcasecmp(formatName, "pascal") == 0)
        format = DiskImg::kFormatPascal;
    else {
        fprintf(stderr, "ERROR: invalid format '%s'\n", formatName);
        return -1;
    }

    if (strcasecmp(sizeStr, "140k") == 0)
        blockCount = 280;
    else if (strcasecmp(sizeStr, "800k") == 0)
        blockCount = 1600;
    else {
        blockCount = atoi(sizeStr);
        if (blockCount <= 0 || blockCount > 65536) {
            fprintf(stderr, "ERROR: invalid size '%s'\n", sizeStr);
            return -1;
        }
    }

    if (access(outputFileName, F_OK) == 0) {
        fprintf(stderr, "ERROR: output file '%s' already exists\n",
            outputFileName);
        return -1;
    }

    assert(argc >= 1);
    assert(*argv != nil);


    const char* volName;
    DiskImg* pDiskImg;
    DiskFS* pDiskFS;
    DIError dierr;

    /*
     * Prepare the disk image file.
     */
    pDiskImg = CreateDisk(outputFileName, blockCount);
    if (pDiskImg == nil)
        return -1;

    if (format == DiskImg::kFormatDOS33)
        volName = "DOS";        // put DOS 3.3 in tracks 0-2
    else
        volName = "TEST";
    dierr = pDiskImg->FormatImage(format, volName);
    if (dierr != kDIErrNone) {
        fprintf(stderr, "ERROR: unable to format disk: %s\n",
            DIStrError(dierr));
        delete pDiskImg;
        return -1;
    }

    /*
     * Prepare to access the image as a filesystem.
     */
    pDiskFS = pDiskImg->OpenAppropriateDiskFS(false);
    if (pDiskFS == nil) {
        fprintf(stderr, "ERROR: unable to open appropriate DiskFS\n");
        delete pDiskImg;
        return -1;
    }

    dierr = pDiskFS->Initialize(pDiskImg, DiskFS::kInitFull);
    if (dierr != kDIErrNone) {
        fprintf(stderr, "ERROR: unable to initialize DiskFS: %s\n",
            DIStrError(dierr));
        delete pDiskFS;
        delete pDiskImg;
        return -1;
    }

    /*
     * Copy the files over.
     */
    if (CopyFiles(pDiskFS, argc, argv) != 0) {
        delete pDiskFS;
        delete pDiskImg;
        return -1;
    }

    /*
     * Clean up.  Note "CloseImage" isn't strictly necessary, but it gives
     * us an opportunity to detect failures.
     */
    delete pDiskFS;

    if (pDiskImg->CloseImage() != 0) {
        fprintf(stderr, "WARNING: CloseImage failed: %s\n",
            DIStrError(dierr));
    }

    delete pDiskImg;
    return 0;
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
    printf("Log file is '%s'\n", kLogFile);
#endif

    Global::SetDebugMsgHandler(MsgHandler);
    Global::AppInit();

    if (argc < 5) {
        Usage(argv[0]);
        exit(2);
    }

    const char* formatName;
    const char* sizeStr;
    const char* outputFileName;

    argv++;
    formatName = *argv++;
    sizeStr = *argv++;
    outputFileName = *argv++;
    argc -= 4;

    if (Process(formatName, sizeStr, outputFileName, argc, argv) == 0)
        fprintf(stderr, "Success!\n");
    else
        fprintf(stderr, "Failed.\n");

    Global::AppCleanup();
#ifdef _DEBUG
    fclose(gLog);
#endif

    exit(0);
}

