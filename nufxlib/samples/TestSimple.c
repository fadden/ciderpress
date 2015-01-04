/*
 * NuFX archive manipulation library
 * Copyright (C) 2000-2007 by Andy McFadden, All Rights Reserved.
 * This is free software; you can redistribute it and/or modify it under the
 * terms of the BSD License, see the file COPYING.LIB.
 *
 * Simple test program.  Opens an archive, dumps the contents.
 *
 * If the first argument is "-", this will read from stdin.  Otherwise,
 * the first argument is taken to be an archive filename, and opened.
 */
#include <stdio.h>
#include "NufxLib.h"
#include "Common.h"


/*
 * Callback function to display the contents of a single record.
 *
 * "pRecord->filename" is the record's filename, whether from the record
 * header, a filename thread, or a default value ("UNKNOWN", stuffed in
 * when a record has no filename at all).
 */
NuResult ShowContents(NuArchive* pArchive, void* vpRecord)
{
    const NuRecord* pRecord = (NuRecord*) vpRecord;

    size_t bufLen = NuConvertMORToUNI(pRecord->filenameMOR, NULL, 0);
    if (bufLen == (size_t) -1) {
        fprintf(stderr, "GLITCH: unable to convert '%s'\n",
            pRecord->filenameMOR);
    } else {
        UNICHAR* buf = (UNICHAR*) malloc(bufLen);
        NuConvertMORToUNI(pRecord->filenameMOR, buf, bufLen);
        printf("*** Filename = '%s'\n", buf);
        free(buf);
    }

    return kNuOK;
}


/*
 * Dump the contents from the streaming input.
 *
 * If we're not interested in handling an archive on stdin, we could just
 * pass the filename in here and use NuOpenRO instead.
 */
int DoStreamStuff(FILE* fp)
{
    NuError err;
    NuArchive* pArchive = NULL;

    err = NuStreamOpenRO(fp, &pArchive);
    if (err != kNuErrNone) {
        fprintf(stderr, "ERROR: unable to open stream archive (err=%d)\n", err);
        goto bail;
    }

    printf("*** Streaming contents!\n");

    err = NuContents(pArchive, ShowContents);
    if (err != kNuErrNone) {
        fprintf(stderr, "ERROR: NuContents failed (err=%d)\n", err);
        goto bail;
    }

bail:
    if (pArchive != NULL) {
        NuError err2 = NuClose(pArchive);
        if (err == kNuErrNone)
            err = err2;
    }

    return err;
}


/*
 * Grab the name of an archive to read.  If "-" was given, use stdin.
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

    if (argc != 2) {
        fprintf(stderr, "Usage: %s (archive-name|-)\n", argv[0]);
        exit(2);
    }

    if (strcmp(argv[1], "-") == 0)
        infp = stdin;
    else {
        infp = fopen(argv[1], kNuFileOpenReadOnly);
        if (infp == NULL) {
            fprintf(stderr, "ERROR: unable to open '%s'\n", argv[1]);
            exit(1);
        }
    }

    cc = DoStreamStuff(infp);
    exit(cc != 0);
}

