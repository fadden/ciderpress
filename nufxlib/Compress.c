/*
 * NuFX archive manipulation library
 * Copyright (C) 2000-2007 by Andy McFadden, All Rights Reserved.
 * This is free software; you can redistribute it and/or modify it under the
 * terms of the BSD License, see the file COPYING-LIB.
 *
 * Compress data into an archive.
 */
#include "NufxLibPriv.h"

/* for ShrinkIt-mimic mode, don't compress files under 512 bytes */
#define kNuSHKLZWThreshold  512


/*
 * "Compress" an uncompressed thread.
 */
static NuError
Nu_CompressUncompressed(NuArchive* pArchive, NuStraw* pStraw,
    FILE* fp, ulong srcLen, ulong* pDstLen, ushort *pCrc)
{
    NuError err = kNuErrNone;
    /*uchar* buffer = nil;*/
    ulong count, getsize;

    Assert(pArchive != nil);
    Assert(pStraw != nil);
    Assert(fp != nil);
    Assert(srcLen > 0);

    *pDstLen = srcLen;  /* get this over with */

    err = Nu_AllocCompressionBufferIFN(pArchive);
    BailError(err);

    if (pCrc != nil)
        *pCrc = kNuInitialThreadCRC;
    count = srcLen;

    while (count) {
        getsize = (count > kNuGenCompBufSize) ? kNuGenCompBufSize : count;

        err = Nu_StrawRead(pArchive, pStraw, pArchive->compBuf, getsize);
        BailError(err);
        if (pCrc != nil)
            *pCrc = Nu_CalcCRC16(*pCrc, pArchive->compBuf, getsize);
        err = Nu_FWrite(fp, pArchive->compBuf, getsize);
        BailError(err);

        count -= getsize;
    }

bail:
    /*Nu_Free(pArchive, buffer);*/
    return err;
}


/*
 * Compress from a data source to an archive.
 *
 * All archive-specified fields in "pThread" will be filled in, as will
 * "actualThreadEOF".  The "nuThreadIdx" and "fileOffset" fields will
 * not be modified, and must be specified before calling here.
 *
 * If "sourceFormat" is uncompressed:
 *  "targetFormat" will be used to compress the data
 *  the data source length will be placed into pThread->thThreadEOF
 *  the compressed size will be placed into pThread->thCompThreadEOF
 *  the CRC is computed
 *
 * If "sourceFormat" is compressed:
 *  the data will be copied without compression (targetFormat is ignored)
 *  the data source "otherLen" value will be placed into pThread->thThreadEOF
 *  the data source length will be placed into pThread->thCompThreadEOF
 *  the CRC is retrieved from Nu_DataSourceGetRawCrc
 *
 * The actual format used will be placed in pThread->thThreadFormat, and
 * the CRC of the uncompressed data will be placed in pThread->thThreadCRC.
 * The remaining fields of "pThread", thThreadClass and thThreadKind, will
 * be set based on the fields in "pDataSource".
 *
 * Data will be written to "dstFp", which must be positioned at the
 * correct point in the output.  The position is expected to match
 * pThread->fileOffset.
 *
 * On exit, the output file will be positioned after the last byte of the
 * output.  (For a pre-sized buffer, this may not be the desired result.)
 */
NuError
Nu_CompressToArchive(NuArchive* pArchive, NuDataSource* pDataSource,
    NuThreadID threadID, NuThreadFormat sourceFormat,
    NuThreadFormat targetFormat, NuProgressData* pProgressData, FILE* dstFp,
    NuThread* pThread)
{
    NuError err;
    long origOffset;
    NuStraw* pStraw = nil;
    NuDataSink* pDataSink = nil;
    ulong srcLen = 0, dstLen = 0;
    ushort threadCrc;

    Assert(pArchive != nil);
    Assert(pDataSource != nil);
    /* okay if pProgressData is nil */
    Assert(dstFp != nil);
    Assert(pThread != nil);

    /* remember file offset, so we can back up if compression fails */
    err = Nu_FTell(dstFp, &origOffset);
    BailError(err);
    Assert(origOffset == pThread->fileOffset);  /* can get rid of ftell? */

    /* fill in some thread fields */
    threadCrc = kNuInitialThreadCRC;

    pThread->thThreadClass = NuThreadIDGetClass(threadID);
    pThread->thThreadKind = NuThreadIDGetKind(threadID);
    pThread->actualThreadEOF = (ulong)-1;
    /* nuThreadIdx and fileOffset should already be set */

    /*
     * Get the input length.  For "buffer" and "fp" sources, this is just
     * a value passed in.  For "file" sources, this is the length of the
     * file on disk.  The file should already have been opened successfully
     * by the caller.
     *
     * If the input file is zero bytes long, "store" it uncompressed and
     * bail immediately.
     *
     * (Our desire to store uncompressible data without compression clashes
     * with a passing interest in doing CRLF conversions on input data.  We
     * want to know the length ahead of time, which potentially makes the
     * compression code simpler, but prevents us from doing the conversion
     * unless we pre-flight the conversion with a separate pass through the
     * input file. Of course, it's still possible for the application to
     * convert the file into a temp file and add from there, so all is
     * not lost.)
     */
    srcLen = Nu_DataSourceGetDataLen(pDataSource);
    /*DBUG(("+++ input file length is %lu\n", srcLen));*/

    /*
     * Create a "Straw" to slurp the input through and track progress.
     */
    err = Nu_StrawNew(pArchive, pDataSource, pProgressData, &pStraw);
    BailError(err);

    if (!srcLen) {
        /* empty file! */
        if (sourceFormat != kNuThreadFormatUncompressed) {
            DBUG(("ODD: empty source is compressed?\n"));
        }
        pThread->thThreadFormat = kNuThreadFormatUncompressed;
        pThread->thThreadCRC = threadCrc;
        pThread->thThreadEOF = 0;
        pThread->thCompThreadEOF = 0;
        pThread->actualThreadEOF = 0;
        goto done;  /* send final progress message */
    }

    if (sourceFormat == kNuThreadFormatUncompressed) {
        /*
         * Compress the input to the requested target format.
         */

        /* for some reason, GSHK doesn't compress anything under 512 bytes */
        if (pArchive->valMimicSHK && srcLen < kNuSHKLZWThreshold)
            targetFormat = kNuThreadFormatUncompressed;

        if (pProgressData != nil) {
            if (targetFormat != kNuThreadFormatUncompressed)
                Nu_StrawSetProgressState(pStraw, kNuProgressCompressing);
            else
                Nu_StrawSetProgressState(pStraw, kNuProgressStoring);
        }
        err = Nu_ProgressDataCompressPrep(pArchive, pStraw, targetFormat,
                srcLen);
        BailError(err);

        switch (targetFormat) {
        case kNuThreadFormatUncompressed:
            err = Nu_CompressUncompressed(pArchive, pStraw, dstFp, srcLen,
                    &dstLen, &threadCrc);
            break;
        #ifdef ENABLE_SQ
        case kNuThreadFormatHuffmanSQ:
            err = Nu_CompressHuffmanSQ(pArchive, pStraw, dstFp, srcLen,
                    &dstLen, &threadCrc);
            break;
        #endif
        #ifdef ENABLE_LZW
        case kNuThreadFormatLZW1:
            err = Nu_CompressLZW1(pArchive, pStraw, dstFp, srcLen, &dstLen,
                    &threadCrc);
            break;
        case kNuThreadFormatLZW2:
            err = Nu_CompressLZW2(pArchive, pStraw, dstFp, srcLen, &dstLen,
                    &threadCrc);
            break;
        #endif
        #ifdef ENABLE_LZC
        case kNuThreadFormatLZC12:
            err = Nu_CompressLZC12(pArchive, pStraw, dstFp, srcLen, &dstLen,
                    &threadCrc);
            break;
        case kNuThreadFormatLZC16:
            err = Nu_CompressLZC16(pArchive, pStraw, dstFp, srcLen, &dstLen,
                    &threadCrc);
            break;
        #endif
        #ifdef ENABLE_DEFLATE
        case kNuThreadFormatDeflate:
            err = Nu_CompressDeflate(pArchive, pStraw, dstFp, srcLen, &dstLen,
                    &threadCrc);
            break;
        #endif
        #ifdef ENABLE_BZIP2
        case kNuThreadFormatBzip2:
            err = Nu_CompressBzip2(pArchive, pStraw, dstFp, srcLen, &dstLen,
                    &threadCrc);
            break;
        #endif
        default:
            /* should've been blocked in Value.c */
            Assert(0);
            err = kNuErrInternal;
            goto bail;
        }

        BailError(err);

        pThread->thThreadCRC = threadCrc;   /* CRC of uncompressed data */

        if (dstLen < srcLen ||
            (dstLen == srcLen && targetFormat == kNuThreadFormatUncompressed))
        {
            /* got smaller, or we didn't try to compress it; keep it */
            pThread->thThreadEOF = srcLen;
            pThread->thCompThreadEOF = dstLen;
            pThread->thThreadFormat = targetFormat;
        } else {
            /* got bigger, store it uncompressed */
            err = Nu_FSeek(dstFp, origOffset, SEEK_SET);
            BailError(err);
            err = Nu_StrawRewind(pArchive, pStraw);
            BailError(err);
            if (pProgressData != nil)
                Nu_StrawSetProgressState(pStraw, kNuProgressStoring);
            err = Nu_ProgressDataCompressPrep(pArchive, pStraw,
                    kNuThreadFormatUncompressed, srcLen);
            BailError(err);

            DBUG(("--- compression (%d) failed (%ld vs %ld), storing\n",
                targetFormat, dstLen, srcLen));
            err = Nu_CompressUncompressed(pArchive, pStraw, dstFp, srcLen,
                    &dstLen, &threadCrc);
            BailError(err);

            /*
             * This holds so long as the previous attempt at compressing
             * computed a CRC on the entire file (i.e. didn't stop early
             * when it noticed the output was larger than the input).  If
             * this is always the case, then we can change "&threadCrc"
             * a few lines back to "nil" and avoid re-computing the CRC.
             * If this is not always the case, remove this assert.
             */
            Assert(threadCrc == pThread->thThreadCRC);

            pThread->thThreadEOF = srcLen;
            pThread->thCompThreadEOF = dstLen;
            pThread->thThreadFormat = kNuThreadFormatUncompressed;
        }

    } else {
        /*
         * Copy the already-compressed input.
         */
        if (pProgressData != nil)
            Nu_StrawSetProgressState(pStraw, kNuProgressCopying);
        err = Nu_ProgressDataCompressPrep(pArchive, pStraw,
                kNuThreadFormatUncompressed, srcLen);
        BailError(err);

        err = Nu_CompressUncompressed(pArchive, pStraw, dstFp, srcLen,
                &dstLen, nil);
        BailError(err);

        pThread->thThreadEOF = Nu_DataSourceGetOtherLen(pDataSource);
        pThread->thCompThreadEOF = srcLen;
        pThread->thThreadFormat = sourceFormat;
        pThread->thThreadCRC = Nu_DataSourceGetRawCrc(pDataSource);
    }
    pThread->actualThreadEOF = pThread->thThreadEOF;

done:
    DBUG(("+++ srcLen=%ld, dstLen=%ld, actual=%ld\n",
        srcLen, dstLen, pThread->actualThreadEOF));

    /* make sure we send a final "success" progress message at 100% */
    if (pProgressData != nil) {
        (void) Nu_StrawSetProgressState(pStraw, kNuProgressDone);
        err = Nu_StrawSendProgressUpdate(pArchive, pStraw);
        BailError(err);
    }

bail:
    (void) Nu_StrawFree(pArchive, pStraw);
    (void) Nu_DataSinkFree(pDataSink);
    return err;
}


/*
 * Copy pre-sized data into the archive at the current offset.
 *
 * All archive-specified fields in "pThread" will be filled in, as will
 * "actualThreadEOF".  The "nuThreadIdx" and "fileOffset" fields will
 * not be modified.
 *
 * Pre-sized data is always uncompressed, and doesn't have a CRC.  This
 * will copy the data, and then continue writing zeros to fill out the rest
 * of the pre-sized buffer.
 */
NuError
Nu_CopyPresizedToArchive(NuArchive* pArchive, NuDataSource* pDataSource,
    NuThreadID threadID, FILE* dstFp, NuThread* pThread, char** ppSavedCopy)
{
    NuError err = kNuErrNone;
    NuStraw* pStraw = nil;
    ulong srcLen, bufferLen;
    ulong count, getsize;

    srcLen = Nu_DataSourceGetDataLen(pDataSource);
    bufferLen = Nu_DataSourceGetOtherLen(pDataSource);
    if (bufferLen < srcLen) {
        /* hey, this won't fit! */
        DBUG(("--- can't fit %lu into buffer of %lu!\n", srcLen, bufferLen));
        err = kNuErrPreSizeOverflow;
        goto bail;
    }
    DBUG(("+++ copying %lu into buffer of %lu\n", srcLen, bufferLen));

    pThread->thThreadClass = NuThreadIDGetClass(threadID);
    pThread->thThreadFormat = kNuThreadFormatUncompressed;
    pThread->thThreadKind = NuThreadIDGetKind(threadID);
    pThread->thThreadCRC = 0;       /* no CRC on pre-sized stuff */
    pThread->thThreadEOF = srcLen;
    pThread->thCompThreadEOF = bufferLen;
    pThread->actualThreadEOF = srcLen;
    /* nuThreadIdx and fileOffset should already be set */

    /*
     * Prepare to copy the data through a buffer.  The "straw" thing
     * is a convenient way to deal with the dataSource, even though we
     * don't have a progress updater.
     */
    err = Nu_StrawNew(pArchive, pDataSource, nil, &pStraw);
    BailError(err);

    count = srcLen;
    err = Nu_AllocCompressionBufferIFN(pArchive);
    BailError(err);

    while (count) {
        getsize = (count > kNuGenCompBufSize) ? kNuGenCompBufSize : count;

        err = Nu_StrawRead(pArchive, pStraw, pArchive->compBuf, getsize);
        BailError(err);
        err = Nu_FWrite(dstFp, pArchive->compBuf, getsize);
        BailError(err);

        if (ppSavedCopy != nil && *ppSavedCopy == nil) {
            /*
             * Grab a copy of the filename for our own use.  This assumes
             * that the filename fits in kNuGenCompBufSize, which is a
             * pretty safe thing to assume.
             */
            Assert(threadID == kNuThreadIDFilename);
            Assert(count == getsize);
            *ppSavedCopy = Nu_Malloc(pArchive, getsize+1);
            BailAlloc(*ppSavedCopy);
            memcpy(*ppSavedCopy, pArchive->compBuf, getsize);
            (*ppSavedCopy)[getsize] = '\0'; /* make sure it's terminated */
        }

        count -= getsize;
    }

    /*
     * Pad out the rest of the buffer.  Could probably do this more
     * efficiently through the buffer we've allocated, but these regions
     * tend to be either 32 or 200 bytes.
     */
    count = bufferLen - srcLen;
    while (count--)
        Nu_WriteOne(pArchive, dstFp, 0);

bail:
    (void) Nu_StrawFree(pArchive, pStraw);
    /*Nu_Free(pArchive, buffer);*/
    return err;
}

