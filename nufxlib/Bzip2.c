/*
 * NuFX archive manipulation library
 * Copyright (C) 2000-2007 by Andy McFadden, All Rights Reserved.
 * This is free software; you can redistribute it and/or modify it under the
 * terms of the BSD License, see the file COPYING-LIB.
 *
 * Support for the "bzip2" (BTW+Huffman) algorithm, via "libbz2".
 *
 * This compression format is totally unsupported on the Apple II.  This
 * is provided primarily for the benefit of Apple II emulators that want
 * a better storage format for disk images than SHK+LZW or a ZIP file.
 *
 * This code was developed and tested with libz2 version 1.0.2.  Visit
 * http://sources.redhat.com/bzip2/ for more information.
 */
#include "NufxLibPriv.h"

#ifdef ENABLE_BZIP2
#include "bzlib.h"

#define kBZBlockSize    8       /* use 800K blocks */
#define kBZVerbosity    1       /* library verbosity level (0-4) */


/*
 * Alloc and free functions provided to libbz2.
 */
static void* Nu_bzalloc(void* opaque, int items, int size)
{
    return Nu_Malloc(opaque, items * size);
}
static void Nu_bzfree(void* opaque, void* address)
{
    Nu_Free(opaque, address);
}


/*
 * ===========================================================================
 *      Compression
 * ===========================================================================
 */

/*
 * Compress "srcLen" bytes from "pStraw" to "fp".
 */
NuError Nu_CompressBzip2(NuArchive* pArchive, NuStraw* pStraw, FILE* fp,
    uint32_t srcLen, uint32_t* pDstLen, uint16_t* pCrc)
{
    NuError err = kNuErrNone;
    bz_stream bzstream;
    int bzerr;
    uint8_t* outbuf = NULL;

    Assert(pArchive != NULL);
    Assert(pStraw != NULL);
    Assert(fp != NULL);
    Assert(srcLen > 0);
    Assert(pDstLen != NULL);
    Assert(pCrc != NULL);

    err = Nu_AllocCompressionBufferIFN(pArchive);
    if (err != kNuErrNone)
        return err;

    /* allocate a similarly-sized buffer for the output */
    outbuf = Nu_Malloc(pArchive, kNuGenCompBufSize);
    BailAlloc(outbuf);

    /*
     * Initialize the bz2lib stream.
     */
    bzstream.bzalloc = Nu_bzalloc;
    bzstream.bzfree = Nu_bzfree;
    bzstream.opaque = pArchive;
    bzstream.next_in = NULL;
    bzstream.avail_in = 0;
    bzstream.next_out = outbuf;
    bzstream.avail_out = kNuGenCompBufSize;

    /* fourth arg is "workFactor"; set to zero for default (30) */
    bzerr = BZ2_bzCompressInit(&bzstream, kBZBlockSize, kBZVerbosity, 0);
    if (bzerr != BZ_OK) {
        err = kNuErrInternal;
        if (bzerr == BZ_CONFIG_ERROR) {
            Nu_ReportError(NU_BLOB, err, "error configuring bz2lib");
        } else {
            Nu_ReportError(NU_BLOB, err,
                "call to BZ2_bzCompressInit failed (bzerr=%d)", bzerr);
        }
        goto bail;
    }

    /*
     * Loop while we have data.
     */
    do {
        uint32_t getSize;
        int action;

        /* should be able to read a full buffer every time */
        if (bzstream.avail_in == 0 && srcLen) {
            getSize = (srcLen > kNuGenCompBufSize) ? kNuGenCompBufSize : srcLen;
            DBUG(("+++ reading %ld bytes\n", getSize));

            err = Nu_StrawRead(pArchive, pStraw, pArchive->compBuf, getSize);
            if (err != kNuErrNone) {
                Nu_ReportError(NU_BLOB, err, "bzip2 read failed");
                goto bz_bail;
            }

            srcLen -= getSize;

            *pCrc = Nu_CalcCRC16(*pCrc, pArchive->compBuf, getSize);

            bzstream.next_in = pArchive->compBuf;
            bzstream.avail_in = getSize;
        }

        if (srcLen == 0)
            action = BZ_FINISH;       /* tell libbz2 that we're done */
        else
            action = BZ_RUN;     /* more to come! */

        bzerr = BZ2_bzCompress(&bzstream, action);
        if (bzerr != BZ_RUN_OK && bzerr != BZ_FINISH_OK && bzerr != BZ_STREAM_END)
        {
            err = kNuErrInternal;
            Nu_ReportError(NU_BLOB, err,
                "libbz2 compress call failed (bzerr=%d)", bzerr);
            goto bz_bail;
        }

        /* write when we're full or when we're done */
        if (bzstream.avail_out == 0 ||
            (bzerr == BZ_STREAM_END && bzstream.avail_out != kNuGenCompBufSize))
        {
            DBUG(("+++ writing %d bytes\n",
                (uint8_t*)bzstream.next_out - outbuf));
            err = Nu_FWrite(fp, outbuf, (uint8_t*)bzstream.next_out - outbuf);
            if (err != kNuErrNone) {
                Nu_ReportError(NU_BLOB, err, "fwrite failed in bzip2");
                goto bz_bail;
            }

            bzstream.next_out = outbuf;
            bzstream.avail_out = kNuGenCompBufSize;
        }
    } while (bzerr != BZ_STREAM_END);

    *pDstLen = bzstream.total_out_lo32;
    Assert(bzstream.total_out_hi32 == 0);   /* no huge files for us */

bz_bail:
    BZ2_bzCompressEnd(&bzstream);       /* free up any allocated structures */

bail:
    if (outbuf != NULL)
        Nu_Free(NULL, outbuf);
    return err;
}


/*
 * ===========================================================================
 *      Expansion
 * ===========================================================================
 */

/*
 * Expand from "infp" to "pFunnel".
 */
NuError Nu_ExpandBzip2(NuArchive* pArchive, const NuRecord* pRecord,
    const NuThread* pThread, FILE* infp, NuFunnel* pFunnel, uint16_t* pCrc)
{
    NuError err = kNuErrNone;
    bz_stream bzstream;
    int bzerr;
    uint32_t compRemaining;
    uint8_t* outbuf;

    Assert(pArchive != NULL);
    Assert(pThread != NULL);
    Assert(infp != NULL);
    Assert(pFunnel != NULL);

    err = Nu_AllocCompressionBufferIFN(pArchive);
    if (err != kNuErrNone)
        return err;

    /* allocate a similarly-sized buffer for the output */
    outbuf = Nu_Malloc(pArchive, kNuGenCompBufSize);
    BailAlloc(outbuf);

    compRemaining = pThread->thCompThreadEOF;

    /*
     * Initialize the libbz2 stream.
     */
    bzstream.bzalloc = Nu_bzalloc;
    bzstream.bzfree = Nu_bzfree;
    bzstream.opaque = pArchive;
    bzstream.next_in = NULL;
    bzstream.avail_in = 0;
    bzstream.next_out = outbuf;
    bzstream.avail_out = kNuGenCompBufSize;

    /* third arg is "small" (set nonzero to reduce mem) */
    bzerr = BZ2_bzDecompressInit(&bzstream, kBZVerbosity, 0);
    if (bzerr != BZ_OK) {
        err = kNuErrInternal;
        if (bzerr == BZ_CONFIG_ERROR) {
            Nu_ReportError(NU_BLOB, err, "error configuring libbz2");
        } else {
            Nu_ReportError(NU_BLOB, err,
                "call to BZ2_bzDecompressInit failed (bzerr=%d)", bzerr);
        }
        goto bail;
    }

    /*
     * Loop while we have data.
     */
    do {
        uint32_t getSize;

        /* read as much as we can */
        if (bzstream.avail_in == 0) {
            getSize = (compRemaining > kNuGenCompBufSize) ?
                        kNuGenCompBufSize : compRemaining;
            DBUG(("+++ reading %ld bytes (%ld left)\n", getSize,
                compRemaining));

            err = Nu_FRead(infp, pArchive->compBuf, getSize);
            if (err != kNuErrNone) {
                Nu_ReportError(NU_BLOB, err, "bzip2 read failed");
                goto bz_bail;
            }

            compRemaining -= getSize;

            bzstream.next_in = pArchive->compBuf;
            bzstream.avail_in = getSize;
        }

        /* uncompress the data */
        bzerr = BZ2_bzDecompress(&bzstream);
        if (bzerr != BZ_OK && bzerr != BZ_STREAM_END) {
            err = kNuErrInternal;
            Nu_ReportError(NU_BLOB, err,
                "libbz2 decompress call failed (bzerr=%d)", bzerr);
            goto bz_bail;
        }

        /* write every time there's anything (buffer will usually be full) */
        if (bzstream.avail_out != kNuGenCompBufSize) {
            DBUG(("+++ writing %d bytes\n",
                (uint8_t*) bzstream.next_out - outbuf));
            err = Nu_FunnelWrite(pArchive, pFunnel, outbuf,
                    (uint8_t*)bzstream.next_out - outbuf);
            if (err != kNuErrNone) {
                Nu_ReportError(NU_BLOB, err, "write failed in bzip2");
                goto bz_bail;
            }

            if (pCrc != NULL)
                *pCrc = Nu_CalcCRC16(*pCrc, outbuf,
                                    (uint8_t*) bzstream.next_out - outbuf);

            bzstream.next_out = outbuf;
            bzstream.avail_out = kNuGenCompBufSize;
        }
    } while (bzerr == BZ_OK);

    Assert(bzerr == BZ_STREAM_END);     /* other errors should've been caught */

    Assert(bzstream.total_out_hi32 == 0);   /* no huge files for us */

    if (bzstream.total_out_lo32 != pThread->actualThreadEOF) {
        err = kNuErrBadData;
        Nu_ReportError(NU_BLOB, err,
            "size mismatch on expanded bzip2 file (%d vs %ld)",
            bzstream.total_out_lo32, pThread->actualThreadEOF);
        goto bz_bail;
    }

bz_bail:
    BZ2_bzDecompressEnd(&bzstream);     /* free up any allocated structures */

bail:
    if (outbuf != NULL)
        Nu_Free(NULL, outbuf);
    return err;
}

#endif /*ENABLE_BZIP2*/
