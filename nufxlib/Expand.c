/*
 * NuFX archive manipulation library
 * Copyright (C) 2000-2007 by Andy McFadden, All Rights Reserved.
 * This is free software; you can redistribute it and/or modify it under the
 * terms of the BSD License, see the file COPYING-LIB.
 *
 * Expand a thread from an archive.
 */
#include "NufxLibPriv.h"


/*
 * "Expand" an uncompressed thread.
 */
static NuError Nu_ExpandUncompressed(NuArchive* pArchive,
    const NuRecord* pRecord, const NuThread* pThread, FILE* infp,
    NuFunnel* pFunnel, uint16_t* pCrc)
{
    NuError err;
    /*uint8_t* buffer = NULL;*/
    uint32_t count, getsize;

    Assert(pArchive != NULL);
    Assert(pThread != NULL);
    Assert(infp != NULL);
    Assert(pFunnel != NULL);

    /* doesn't have to be same size as funnel, but it's not a bad idea */
    /*buffer = Nu_Malloc(pArchive, kNuFunnelBufSize);*/
    /*BailAlloc(buffer);*/
    err = Nu_AllocCompressionBufferIFN(pArchive);
    BailError(err);

    /* quick assert for bad archive that should have been caught earlier */
    /* (filename threads are uncompressed, but compThreadEOF is buf len) */
    if (pThread->thThreadClass == kNuThreadClassData)
        Assert(pThread->actualThreadEOF == pThread->thCompThreadEOF);

    count = pThread->actualThreadEOF;

    while (count) {
        getsize = (count > kNuGenCompBufSize) ? kNuGenCompBufSize : count;

        err = Nu_FRead(infp, pArchive->compBuf, getsize);
        BailError(err);
        if (pCrc != NULL)
            *pCrc = Nu_CalcCRC16(*pCrc, pArchive->compBuf, getsize);
        err = Nu_FunnelWrite(pArchive, pFunnel, pArchive->compBuf, getsize);
        BailError(err);

        count -= getsize;
    }

    err = Nu_FunnelFlush(pArchive, pFunnel);
    BailError(err);

bail:
    /*Nu_Free(pArchive, buffer);*/
    return err;
}

/*
 * Copy the "raw" data out of the thread.  Unlike the preceeding function,
 * this reads up to "thCompThreadEOF", and doesn't even try to compute a CRC.
 */
static NuError Nu_ExpandRaw(NuArchive* pArchive, const NuThread* pThread,
    FILE* infp, NuFunnel* pFunnel)
{
    NuError err;
    /*uint8_t* buffer = NULL;*/
    uint32_t count, getsize;

    Assert(pArchive != NULL);
    Assert(pThread != NULL);
    Assert(infp != NULL);
    Assert(pFunnel != NULL);

    /* doesn't have to be same size as funnel, but it's not a bad idea */
    /*buffer = Nu_Malloc(pArchive, kNuFunnelBufSize);*/
    /*BailAlloc(buffer);*/
    err = Nu_AllocCompressionBufferIFN(pArchive);
    BailError(err);

    count = pThread->thCompThreadEOF;

    while (count) {
        getsize = (count > kNuGenCompBufSize) ? kNuGenCompBufSize : count;

        err = Nu_FRead(infp, pArchive->compBuf, getsize);
        BailError(err);
        err = Nu_FunnelWrite(pArchive, pFunnel, pArchive->compBuf, getsize);
        BailError(err);

        count -= getsize;
    }

    err = Nu_FunnelFlush(pArchive, pFunnel);
    BailError(err);

bail:
    /*Nu_Free(pArchive, buffer);*/
    return err;
}


/*
 * Expand a thread from "infp" to "pFunnel", using the compression
 * and stream length specified by "pThread".
 */
NuError Nu_ExpandStream(NuArchive* pArchive, const NuRecord* pRecord,
    const NuThread* pThread, FILE* infp, NuFunnel* pFunnel)
{
    NuError err = kNuErrNone;
    uint16_t calcCrc;
    uint16_t* pCalcCrc;

    if (!pThread->thThreadEOF && !pThread->thCompThreadEOF) {
        /* somebody stored an empty file! */
        goto done;
    }

    /*
     * A brief history of the "threadCRC" field in the thread header:
     *  record versions 0 and 1 didn't use the threadCRC field
     *  record version 2 put the CRC of the compressed data in threadCRC
     *  record version 3 put the CRC of the uncompressed data in threadCRC
     *
     * P8 ShrinkIt uses v1, GSHK uses v3.  If something ever shipped with
     * v2, it didn't last long enough to leave an impression, so I'm not
     * going to support it.  BTW, P8 ShrinkIt always uses LZW/1, which
     * puts a CRC in the compressed stream.  Your uncompressed data is,
     * unfortunately, unprotected before v3.
     */
    calcCrc = kNuInitialThreadCRC;
    pCalcCrc = NULL;
    if (Nu_ThreadHasCRC(pRecord->recVersionNumber, NuGetThreadID(pThread)) &&
        !pArchive->valIgnoreCRC)
    {
        pCalcCrc = &calcCrc;
    }

    err = Nu_ProgressDataExpandPrep(pArchive, pFunnel, pThread);
    BailError(err);

    /*
     * If we're not expanding the data, use a simple copier.
     */
    if (!Nu_FunnelGetDoExpand(pFunnel)) {
        Nu_FunnelSetProgressState(pFunnel, kNuProgressCopying);
        err = Nu_ExpandRaw(pArchive, pThread, infp, pFunnel);
        BailError(err);
        goto done;
    }

    Nu_FunnelSetProgressState(pFunnel, kNuProgressExpanding);
    switch (pThread->thThreadFormat) {
    case kNuThreadFormatUncompressed:
        Nu_FunnelSetProgressState(pFunnel, kNuProgressCopying);
        err = Nu_ExpandUncompressed(pArchive, pRecord, pThread, infp, pFunnel,
                pCalcCrc);
        break;
    #ifdef ENABLE_SQ
    case kNuThreadFormatHuffmanSQ:
        err = Nu_ExpandHuffmanSQ(pArchive, pRecord, pThread, infp, pFunnel,
                pCalcCrc);
        break;
    #endif
    #ifdef ENABLE_LZW
    case kNuThreadFormatLZW1:
    case kNuThreadFormatLZW2:
        err = Nu_ExpandLZW(pArchive, pRecord, pThread, infp, pFunnel, pCalcCrc);
        break;
    #endif
    #ifdef ENABLE_LZC
    case kNuThreadFormatLZC12:
    case kNuThreadFormatLZC16:
        err = Nu_ExpandLZC(pArchive, pRecord, pThread, infp, pFunnel, pCalcCrc);
        break;
    #endif
    #ifdef ENABLE_DEFLATE
    case kNuThreadFormatDeflate:
        err = Nu_ExpandDeflate(pArchive, pRecord, pThread, infp, pFunnel,
                pCalcCrc);
        break;
    #endif
    #ifdef ENABLE_BZIP2
    case kNuThreadFormatBzip2:
        err = Nu_ExpandBzip2(pArchive, pRecord, pThread, infp, pFunnel,
                pCalcCrc);
        break;
    #endif
    default:
        err = kNuErrBadFormat;
        Nu_ReportError(NU_BLOB, err,
            "compression format %u not supported", pThread->thThreadFormat);
        break;
    }
    BailError(err);

    err = Nu_FunnelFlush(pArchive, pFunnel);
    BailError(err);

    /*
     * If we have a CRC to check, check it.
     */
    if (pCalcCrc != NULL) {
        if (calcCrc != pThread->thThreadCRC) {
            if (!Nu_ShouldIgnoreBadCRC(pArchive, pRecord, kNuErrBadThreadCRC)) {
                err = kNuErrBadDataCRC;
                Nu_ReportError(NU_BLOB, err, "expected 0x%04x, got 0x%04x",
                    pThread->thThreadCRC, calcCrc);
                goto bail;
            }
        } else {
            DBUG(("--- thread CRCs match\n"));
        }
    }

done:
    /* make sure we send a final "success" progress message at 100% */
    (void) Nu_FunnelSetProgressState(pFunnel, kNuProgressDone);
    err = Nu_FunnelSendProgressUpdate(pArchive, pFunnel);
    BailError(err);

bail:
    return err;
}

