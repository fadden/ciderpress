/*
 * NuFX archive manipulation library
 * Copyright (C) 2000-2007 by Andy McFadden, All Rights Reserved.
 * This is free software; you can redistribute it and/or modify it under the
 * terms of the BSD License, see the file COPYING-LIB.
 *
 * Support for the "deflate" algorithm, via the "zlib" library.
 *
 * This compression format is totally unsupported on the Apple II.  This
 * is provided primarily for the benefit of Apple II emulators that want
 * a better storage format for disk images than SHK+LZW or a ZIP file.
 *
 * This code was developed and tested with ZLIB_VERSION "1.1.3".  It is
 * expected to work with any version >= 1.1.3 and < 2.x.  Please visit
 * http://www.zlib.org/ for more information.
 */
#include "NufxLibPriv.h"

#ifdef ENABLE_DEFLATE
#include "zlib.h"

#define kNuDeflateLevel 9       /* use maximum compression */


/*
 * Alloc and free functions provided to zlib.
 */
static voidpf
Nu_zalloc(voidpf opaque, uInt items, uInt size)
{
    return Nu_Malloc(opaque, items * size);
}
static void
Nu_zfree(voidpf opaque, voidpf address)
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
NuError
Nu_CompressDeflate(NuArchive* pArchive, NuStraw* pStraw, FILE* fp,
    ulong srcLen, ulong* pDstLen, ushort* pCrc)
{
    NuError err = kNuErrNone;
    z_stream zstream;
    int zerr;
    Bytef* outbuf = nil;

    Assert(pArchive != nil);
    Assert(pStraw != nil);
    Assert(fp != nil);
    Assert(srcLen > 0);
    Assert(pDstLen != nil);
    Assert(pCrc != nil);

    err = Nu_AllocCompressionBufferIFN(pArchive);
    if (err != kNuErrNone)
        return err;

    /* allocate a similarly-sized buffer for the output */
    outbuf = Nu_Malloc(pArchive, kNuGenCompBufSize);
    BailAlloc(outbuf);

    /*
     * Initialize the zlib stream.
     */
    zstream.zalloc = Nu_zalloc;
    zstream.zfree = Nu_zfree;
    zstream.opaque = pArchive;
    zstream.next_in = nil;
    zstream.avail_in = 0;
    zstream.next_out = outbuf;
    zstream.avail_out = kNuGenCompBufSize;
    zstream.data_type = Z_UNKNOWN;

    zerr = deflateInit(&zstream, kNuDeflateLevel);
    if (zerr != Z_OK) {
        err = kNuErrInternal;
        if (zerr == Z_VERSION_ERROR) {
            Nu_ReportError(NU_BLOB, err,
                "installed zlib is not compatible with linked version (%s)",
                ZLIB_VERSION);
        } else {
            Nu_ReportError(NU_BLOB, err,
                "call to deflateInit failed (zerr=%d)", zerr);
        }
        goto bail;
    }

    /*
     * Loop while we have data.
     */
    do {
        ulong getSize;
        int flush;

        /* should be able to read a full buffer every time */
        if (zstream.avail_in == 0 && srcLen) {
            getSize = (srcLen > kNuGenCompBufSize) ? kNuGenCompBufSize : srcLen;
            DBUG(("+++ reading %ld bytes\n", getSize));

            err = Nu_StrawRead(pArchive, pStraw, pArchive->compBuf, getSize);
            if (err != kNuErrNone) {
                Nu_ReportError(NU_BLOB, err, "deflate read failed");
                goto z_bail;
            }

            srcLen -= getSize;

            *pCrc = Nu_CalcCRC16(*pCrc, pArchive->compBuf, getSize);

            zstream.next_in = pArchive->compBuf;
            zstream.avail_in = getSize;
        }

        if (srcLen == 0)
            flush = Z_FINISH;       /* tell zlib that we're done */
        else
            flush = Z_NO_FLUSH;     /* more to come! */

        zerr = deflate(&zstream, flush);
        if (zerr != Z_OK && zerr != Z_STREAM_END) {
            err = kNuErrInternal;
            Nu_ReportError(NU_BLOB, err, "zlib deflate call failed (zerr=%d)",
                zerr);
            goto z_bail;
        }

        /* write when we're full or when we're done */
        if (zstream.avail_out == 0 ||
            (zerr == Z_STREAM_END && zstream.avail_out != kNuGenCompBufSize))
        {
            DBUG(("+++ writing %d bytes\n", zstream.next_out - outbuf));
            err = Nu_FWrite(fp, outbuf, zstream.next_out - outbuf);
            if (err != kNuErrNone) {
                Nu_ReportError(NU_BLOB, err, "fwrite failed in deflate");
                goto z_bail;
            }

            zstream.next_out = outbuf;
            zstream.avail_out = kNuGenCompBufSize;
        }
    } while (zerr == Z_OK);

    Assert(zerr == Z_STREAM_END);       /* other errors should've been caught */

    *pDstLen = zstream.total_out;

z_bail:
    deflateEnd(&zstream);        /* free up any allocated structures */

bail:
    if (outbuf != nil)
        free(outbuf);
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
NuError
Nu_ExpandDeflate(NuArchive* pArchive, const NuRecord* pRecord,
    const NuThread* pThread, FILE* infp, NuFunnel* pFunnel, ushort* pCrc)
{
    NuError err = kNuErrNone;
    z_stream zstream;
    int zerr;
    ulong compRemaining;
    Bytef* outbuf;

    Assert(pArchive != nil);
    Assert(pThread != nil);
    Assert(infp != nil);
    Assert(pFunnel != nil);

    err = Nu_AllocCompressionBufferIFN(pArchive);
    if (err != kNuErrNone)
        return err;

    /* allocate a similarly-sized buffer for the output */
    outbuf = Nu_Malloc(pArchive, kNuGenCompBufSize);
    BailAlloc(outbuf);

    compRemaining = pThread->thCompThreadEOF;

    /*
     * Initialize the zlib stream.
     */
    zstream.zalloc = Nu_zalloc;
    zstream.zfree = Nu_zfree;
    zstream.opaque = pArchive;
    zstream.next_in = nil;
    zstream.avail_in = 0;
    zstream.next_out = outbuf;
    zstream.avail_out = kNuGenCompBufSize;
    zstream.data_type = Z_UNKNOWN;

    zerr = inflateInit(&zstream);
    if (zerr != Z_OK) {
        err = kNuErrInternal;
        if (zerr == Z_VERSION_ERROR) {
            Nu_ReportError(NU_BLOB, err,
                "installed zlib is not compatible with linked version (%s)",
                ZLIB_VERSION);
        } else {
            Nu_ReportError(NU_BLOB, err,
                "call to inflateInit failed (zerr=%d)", zerr);
        }
        goto bail;
    }

    /*
     * Loop while we have data.
     */
    do {
        ulong getSize;

        /* read as much as we can */
        if (zstream.avail_in == 0) {
            getSize = (compRemaining > kNuGenCompBufSize) ?
                        kNuGenCompBufSize : compRemaining;
            DBUG(("+++ reading %ld bytes (%ld left)\n", getSize,
                compRemaining));

            err = Nu_FRead(infp, pArchive->compBuf, getSize);
            if (err != kNuErrNone) {
                Nu_ReportError(NU_BLOB, err, "inflate read failed");
                goto z_bail;
            }

            compRemaining -= getSize;

            zstream.next_in = pArchive->compBuf;
            zstream.avail_in = getSize;
        }

        /* uncompress the data */
        zerr = inflate(&zstream, Z_NO_FLUSH);
        if (zerr != Z_OK && zerr != Z_STREAM_END) {
            err = kNuErrInternal;
            Nu_ReportError(NU_BLOB, err, "zlib inflate call failed (zerr=%d)",
                zerr);
            goto z_bail;
        }

        /* write every time there's anything (buffer will usually be full) */
        if (zstream.avail_out != kNuGenCompBufSize) {
            DBUG(("+++ writing %d bytes\n", zstream.next_out - outbuf));
            err = Nu_FunnelWrite(pArchive, pFunnel, outbuf,
                    zstream.next_out - outbuf);
            if (err != kNuErrNone) {
                Nu_ReportError(NU_BLOB, err, "write failed in inflate");
                goto z_bail;
            }

            if (pCrc != nil)
                *pCrc = Nu_CalcCRC16(*pCrc, outbuf, zstream.next_out - outbuf);

            zstream.next_out = outbuf;
            zstream.avail_out = kNuGenCompBufSize;
        }
    } while (zerr == Z_OK);

    Assert(zerr == Z_STREAM_END);       /* other errors should've been caught */

    if (zstream.total_out != pThread->actualThreadEOF) {
        err = kNuErrBadData;
        Nu_ReportError(NU_BLOB, err,
            "size mismatch on inflated file (%ld vs %ld)",
            zstream.total_out, pThread->actualThreadEOF);
        goto z_bail;
    }

z_bail:
    inflateEnd(&zstream);        /* free up any allocated structures */

bail:
    if (outbuf != nil)
        free(outbuf);
    return err;
}

#endif /*ENABLE_DEFLATE*/
