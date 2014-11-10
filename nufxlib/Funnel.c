/*
 * NuFX archive manipulation library
 * Copyright (C) 2000-2007 by Andy McFadden, All Rights Reserved.
 * This is free software; you can redistribute it and/or modify it under the
 * terms of the BSD License, see the file COPYING-LIB.
 *
 * Implementation of NuFunnel, NuStraw and ProgressUpdater.
 */
#include "NufxLibPriv.h"


/*
 * ===========================================================================
 *      Progress updater
 * ===========================================================================
 */

/*
 * Initialize the fields in a ProgressData structure, prior to compressing
 * data into a record.
 *
 * The same structure will be used when expanding all threads in a given
 * record.
 */
NuError
Nu_ProgressDataInit_Compress(NuArchive* pArchive, NuProgressData* pProgressData,
    const NuRecord* pRecord, const char* origPathname)
{
    const char* cp;

    Assert(pProgressData != nil);
    Assert(pArchive != nil);
    Assert(pRecord != nil);
    Assert(origPathname != nil);

    pProgressData->pRecord = pRecord;

    pProgressData->origPathname = origPathname;
    pProgressData->pathname = pRecord->filename;
    cp = strrchr(pRecord->filename,
            NuGetSepFromSysInfo(pRecord->recFileSysInfo));
    if (cp == nil || *(cp+1) == '\0')
        pProgressData->filename = pProgressData->pathname;
    else
        pProgressData->filename = cp+1;

    pProgressData->operation = kNuOpAdd;
    pProgressData->state = kNuProgressPreparing;
    /*pProgressData->compressedLength = 0;*/
    /*pProgressData->compressedProgress = 0;*/
    pProgressData->uncompressedLength = 0;
    pProgressData->uncompressedProgress = 0;

    pProgressData->compress.threadFormat = (NuThreadFormat)-1;

    /* ya know... if this is nil, none of the above matters much */
    pProgressData->progressFunc = pArchive->progressUpdaterFunc;

    return kNuErrNone;
}


/*
 * Initialize the fields in a ProgressData structure, prior to expanding
 * data from a record.
 *
 * The same structure will be used when expanding all threads in a given
 * record.
 */
NuError
Nu_ProgressDataInit_Expand(NuArchive* pArchive, NuProgressData* pProgressData,
    const NuRecord* pRecord, const char* newPathname, char newFssep,
    NuValue convertEOL)
{
    const NuThread* pThreadIter;
    const char* cp;
    int i;

    Assert(pProgressData != nil);
    Assert(pArchive != nil);
    Assert(pRecord != nil);
    Assert(newPathname != nil);
    Assert(newFssep != 0);

    pProgressData->pRecord = pRecord;
    pProgressData->expand.pThread = nil;

    pProgressData->origPathname = pRecord->filename;
    pProgressData->pathname = newPathname;
    cp = strrchr(newPathname, newFssep);
    if (cp == nil || *(cp+1) == '\0')
        pProgressData->filename = newPathname;
    else
        pProgressData->filename = cp+1;

    pProgressData->expand.convertEOL = convertEOL;

    /* total up the data threads */
    pProgressData->expand.totalCompressedLength = 0;
    pProgressData->expand.totalUncompressedLength = 0;

    for (i = 0; i < (int)pRecord->recTotalThreads; i++) {
        pThreadIter = Nu_GetThread(pRecord, i);
        if (pThreadIter->thThreadClass != kNuThreadClassData)
            continue;
        pProgressData->expand.totalCompressedLength += pThreadIter->thCompThreadEOF;
        pProgressData->expand.totalUncompressedLength += pThreadIter->actualThreadEOF;
    }

    pProgressData->operation = kNuOpExtract;
    if (pArchive->testMode)
        pProgressData->operation = kNuOpTest;
    pProgressData->state = kNuProgressPreparing;
    /*pProgressData->expand.compressedLength = 0;*/
    /*pProgressData->expand.compressedProgress = 0;*/
    pProgressData->uncompressedLength = 0;
    pProgressData->uncompressedProgress = 0;

    /* ya know... if this is nil, none of the above matters much */
    pProgressData->progressFunc = pArchive->progressUpdaterFunc;

    return kNuErrNone;
}


/*
 * Do the setup on a ProgressData prior to compressing a thread.
 */
NuError
Nu_ProgressDataCompressPrep(NuArchive* pArchive, NuStraw* pStraw,
    NuThreadFormat threadFormat, ulong sourceLen)
{
    NuProgressData* pProgressData;

    Assert(pArchive != nil);
    Assert(pStraw != nil);
    Assert(sourceLen < 32767*65536);

    pProgressData = pStraw->pProgress;
    if (pProgressData == nil)
        return kNuErrNone;

    pProgressData->uncompressedLength = sourceLen;
    pProgressData->compress.threadFormat = threadFormat;

    return kNuErrNone;
}

/*
 * Do the setup on a ProgressData prior to expanding a thread.
 *
 * "pThread" is the thread being expanded.
 */
NuError
Nu_ProgressDataExpandPrep(NuArchive* pArchive, NuFunnel* pFunnel,
    const NuThread* pThread)
{
    NuProgressData* pProgressData;

    Assert(pArchive != nil);
    Assert(pFunnel != nil);
    Assert(pThread != nil);

    pProgressData = pFunnel->pProgress;
    if (pProgressData == nil)
        return kNuErrNone;

    /*pProgressData->compressedLength = pThread->thCompThreadEOF;*/
    pProgressData->uncompressedLength = pThread->actualThreadEOF;
    pProgressData->expand.pThread = pThread;

    return kNuErrNone;
}

/*
 * Compute a completion percentage.
 */
static int
Nu_ComputePercent(ulong total, ulong progress)
{
    ulong perc;

    if (!total)
        return 0;

    if (total < 21474836) {
        perc = (progress * 100 + 50) / total;
        if (perc > 100)
            perc = 100;
    } else {
        perc = progress / (total / 100);
        if (perc > 100)
            perc = 100;
    }

    return (int) perc;
}

/*
 * Send the initial progress message, before the output file is opened
 * (when extracting) or the input file is opened (when adding).
 */
NuError
Nu_SendInitialProgress(NuArchive* pArchive, NuProgressData* pProgress)
{
    NuResult result;

    Assert(pArchive != nil);
    Assert(pProgress != nil);

    if (pProgress->progressFunc == nil)
        return kNuErrNone;

    pProgress->percentComplete = Nu_ComputePercent(
        pProgress->uncompressedLength, pProgress->uncompressedProgress);

    result = (*pProgress->progressFunc)(pArchive, (NuProgressData*) pProgress);

    if (result == kNuSkip)
        return kNuErrSkipped;   /* [dunno how well this works] */
    if (result == kNuAbort)
        return kNuErrAborted;

    return kNuErrNone;
}


/*
 * ===========================================================================
 *      NuFunnel object
 * ===========================================================================
 */

/*
 * Allocate and initialize a Funnel.
 */
NuError
Nu_FunnelNew(NuArchive* pArchive, NuDataSink* pDataSink, NuValue convertEOL,
    NuValue convertEOLTo, NuProgressData* pProgress, NuFunnel** ppFunnel)
{
    NuError err = kNuErrNone;
    NuFunnel* pFunnel = nil;

    Assert(ppFunnel != nil);
    Assert(pDataSink != nil);
    Assert(convertEOL == kNuConvertOff ||
           convertEOL == kNuConvertOn ||
           convertEOL == kNuConvertAuto);

    pFunnel = Nu_Calloc(pArchive, sizeof(*pFunnel));
    BailAlloc(pFunnel);
    pFunnel->buffer = Nu_Malloc(pArchive, kNuFunnelBufSize);
    BailAlloc(pFunnel->buffer);

    pFunnel->pDataSink = pDataSink;
    pFunnel->convertEOL = convertEOL;
    pFunnel->convertEOLTo = convertEOLTo;
    pFunnel->convertEOLFrom = kNuEOLUnknown;
    pFunnel->pProgress = pProgress;

    pFunnel->checkStripHighASCII = (pArchive->valStripHighASCII != 0);
    pFunnel->doStripHighASCII = false;  /* determined on first write */

    pFunnel->isFirstWrite = true;

bail:
    if (err != kNuErrNone)
        Nu_FunnelFree(pArchive, pFunnel);
    else
        *ppFunnel = pFunnel;
    return err;
}


/*
 * Free a Funnel.
 *
 * The data should already have been written; it's not the duty of a
 * "free" function to flush data out.
 */
NuError
Nu_FunnelFree(NuArchive* pArchive, NuFunnel* pFunnel)
{
    if (pFunnel == nil)
        return kNuErrNone;

#ifdef DEBUG_MSGS
    if (pFunnel->bufCount)
        Nu_ReportError(NU_BLOB_DEBUG, kNuErrNone,
            "freeing non-empty funnel");
#endif

    Nu_Free(pArchive, pFunnel->buffer);
    Nu_Free(pArchive, pFunnel);

    return kNuErrNone;
}


#if 0
/*
 * Set the maximum amount of output we're willing to push through the
 * funnel.  Attempts to write more than this many bytes will fail.  This
 * allows us to bail out as soon as it's apparent that compression is
 * failing and is actually resulting in a larger file.
 */
void
Nu_FunnelSetMaxOutput(NuFunnel* pFunnel, ulong maxBytes)
{
    Assert(pFunnel != nil);
    Assert(maxBytes > 0);

    pFunnel->outMax = maxBytes;
    if (pFunnel->outCount >= pFunnel->outMax)
        pFunnel->outMaxExceeded = true;
    else
        pFunnel->outMaxExceeded = false;
}
#endif


/*
 * Check to see if this is a high-ASCII file.  To qualify, EVERY
 * character must have its high bit set, except for spaces (0x20).
 * (The exception is courtesy Glen Bredon's "Merlin".)
 */
static Boolean
Nu_CheckHighASCII(const NuFunnel* pFunnel, const unsigned char* buffer,
    unsigned long count)
{
    Boolean isHighASCII;

    Assert(buffer != nil);
    Assert(count != 0);
    Assert(pFunnel->checkStripHighASCII);

    isHighASCII = true;
    while (count--) {
        if ((*buffer & 0x80) == 0 && *buffer != 0x20) {
            isHighASCII = false;
            break;
        }
        
        buffer++;
    }

    return isHighASCII;
}

/*
 * Table determining what's a binary character and what isn't.  It would
 * possibly be more compact to generate this from a simple description,
 * but I'm hoping static/const data will end up in the code segment and
 * save space on the heap.
 *
 * This corresponds to less-316's ISO-latin1 "8bcccbcc18b95.33b.".  This
 * may be too loose by itself; we may want to require that the lower-ASCII
 * values appear in higher proportions than the upper-ASCII values.
 * Otherwise we run the risk of converting a binary file with specific
 * properties.  (Note that "upper-ASCII" refers to umlauts and other
 * accented characters, not DOS 3.3 "high ASCII".)
 *
 * The auto-detect mechanism will never be perfect though, so there's not
 * much point in tweaking it to death.
 */
static const char gNuIsBinary[256] = {
    1, 1, 1, 1, 1, 1, 1, 1,  0, 0, 0, 1, 0, 0, 1, 1,    /* ^@-^O */
    1, 1, 1, 1, 1, 1, 1, 1,  1, 1, 1, 1, 1, 1, 1, 1,    /* ^P-^_ */
    0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0,    /*   - / */
    0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0,    /* 0 - ? */
    0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0,    /* @ - O */
    0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0,    /* P - _ */
    0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0,    /* ` - o */
    0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 1,    /* p - DEL */
    1, 1, 1, 1, 1, 1, 1, 1,  1, 1, 1, 1, 1, 1, 1, 1,    /* 0x80 */
    1, 1, 1, 1, 1, 1, 1, 1,  1, 1, 1, 1, 1, 1, 1, 1,    /* 0x90 */
    0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0,    /* 0xa0 */
    0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0,    /* 0xb0 */
    0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0,    /* 0xc0 */
    0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0,    /* 0xd0 */
    0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0,    /* 0xe0 */
    0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0,    /* 0xf0 */
};

#define kNuMaxUpperASCII    1       /* max #of binary chars per 100 bytes */
#define kNuMinConvThreshold 40      /* min of 40 chars for auto-detect */
/*
 * Decide, based on the contents of the buffer, whether we should do an
 * EOL conversion on the data.
 *
 * We need to decide if we are looking at text data, and if so, what kind
 * of line terminator is in use.
 *
 * If we don't have enough data to make a determination, don't mess with it.
 * (Thought for the day: add a "bias" flag, based on the NuRecord fileType,
 * that causes us to handle borderline or sub-min-threshold cases more
 * reasonably.  If it's of type TXT, it's probably text.)
 *
 * We try to figure out whether it's CR, LF, or CRLF, so that we can
 * skip the CPU-intensive conversion process if it isn't necessary.
 *
 * We will also enable a "high-ASCII" stripper if requested.  This is
 * only enabled when EOL conversions are enabled.
 *
 * Returns kConvEOLOff or kConvEOLOn, and sets pFunnel->doStripHighASCII
 * if pFunnel->CheckStripHighASCII is set.
 */
static NuValue
Nu_DetermineConversion(NuFunnel* pFunnel, const uchar* buffer, ulong count)
{
    ulong bufCount, numBinary, numLF, numCR;
    Boolean isHighASCII;
    uchar val;

    if (count < kNuMinConvThreshold)
        return kNuConvertOff;

    /*
     * Check to see if the buffer is all high-ASCII characters.  If it is,
     * we want to strip characters before we test them below.
     */
    if (pFunnel->checkStripHighASCII) {
        isHighASCII = Nu_CheckHighASCII(pFunnel, buffer, count);
        DBUG(("+++ determined isHighASCII=%d\n", isHighASCII));
    } else {
        isHighASCII = false;
        DBUG(("+++ not even checking isHighASCII\n"));
    }

    bufCount = count;
    numBinary = numLF = numCR = 0;
    while (bufCount--) {
        val = *buffer++;
        if (isHighASCII)
            val &= 0x7f;
        if (gNuIsBinary[val])
            numBinary++;
        if (val == kNuCharLF)
            numLF++;
        if (val == kNuCharCR)
            numCR++;
    }

    /* if #found is > #allowed, it's a binary file */
    if (count < 100) {
        /* use simplified check on files between kNuMinConvThreshold and 100 */
        if (numBinary > kNuMaxUpperASCII)
            return kNuConvertOff;
    } else if (numBinary > (count / 100) * kNuMaxUpperASCII)
        return kNuConvertOff;

    /*
     * If our "convert to" setting is the same as what we're converting
     * from, we can turn off the converter and speed things up.
     *
     * These are simplistic, but this is intended as an optimization.  We
     * will blow it if the input has lots of CRs and LFs scattered about,
     * and they just happen to be in equal amounts, but it's not clear
     * to me that an automatic EOL conversion makes sense on that sort
     * of file anyway.
     *
     * None of this applies if we also need to do a high-ASCII conversion.
     */
    if (isHighASCII) {
        pFunnel->doStripHighASCII = true;
    } else {
        if (numLF && !numCR)
            pFunnel->convertEOLFrom = kNuEOLLF;
        else if (!numLF && numCR)
            pFunnel->convertEOLFrom = kNuEOLCR;
        else if (numLF && numLF == numCR)
            pFunnel->convertEOLFrom = kNuEOLCRLF;
        else
            pFunnel->convertEOLFrom = kNuEOLUnknown;
    }

    return kNuConvertOn;
}

/*
 * Write a block of data to the appropriate output device.  Test for
 * excessive data, and raise "outMaxExceeded" if we overrun.
 *
 * This is either a Funnel function or a DataSink function, depending on
 * your perspective.
 */
static inline void
Nu_FunnelPutBlock(NuFunnel* pFunnel, const uchar* buf, ulong len)
{
    Assert(pFunnel != nil);
    Assert(pFunnel->pDataSink != nil);
    Assert(buf != nil);
    Assert(len > 0);

#if 0
    if (pFunnel->outMax) {
        if (pFunnel->outMaxExceeded)
            return;
        if (pFunnel->outCount + len > pFunnel->outMax) {
            pFunnel->outMaxExceeded = true;
            return;
        }
    }
    pFunnel->outCount += len;
#endif

    Nu_DataSinkPutBlock(pFunnel->pDataSink, buf, len);
}


/*
 * Output the EOL marker requested for this system.
 */
static inline void
Nu_PutEOL(NuFunnel* pFunnel)
{
    uchar ch;

    if (pFunnel->convertEOLTo == kNuEOLCR) {
        ch = kNuCharCR;
        Nu_FunnelPutBlock(pFunnel, &ch, 1);
    } else if (pFunnel->convertEOLTo == kNuEOLLF) {
        ch = kNuCharLF;
        Nu_FunnelPutBlock(pFunnel, &ch, 1);
    } else if (pFunnel->convertEOLTo == kNuEOLCRLF) {
        ch = kNuCharCR;
        Nu_FunnelPutBlock(pFunnel, &ch, 1);
        ch = kNuCharLF;
        Nu_FunnelPutBlock(pFunnel, &ch, 1);
    } else {
        Assert(0);
    }
}

/*
 * Write a buffer of data, using the EOL conversion associated with the
 * funnel (if any).
 *
 * When converting to the system's EOL convention, we take anything
 * that looks like an EOL mark and convert it.  Doesn't matter if it's
 * CR, LF, or CRLF; all three get converted to whatever the system uses.
 */
static NuError
Nu_FunnelWriteConvert(NuFunnel* pFunnel, const uchar* buffer, ulong count)
{
    NuError err = kNuErrNone;
    ulong progressCount = count;

    /*if (pFunnel->outMaxExceeded)
        return kNuErrOutMax;*/

    if (pFunnel->isFirstWrite) {
        /*
         * This is the first write/flush we've done on this Funnel.
         * Check the data we have buffered to decide whether or not
         * we want to do text conversions.
         */
        if (pFunnel->convertEOL == kNuConvertAuto) {
            pFunnel->convertEOL = Nu_DetermineConversion(pFunnel, buffer,count);
            DBUG(("+++ DetermineConversion --> %ld / %ld (%d)\n",
                pFunnel->convertEOL, pFunnel->convertEOLFrom,
                pFunnel->doStripHighASCII));

            if (pFunnel->convertEOLFrom == pFunnel->convertEOLTo) {
                DBUG(("+++ Switching redundant converter off\n"));
                pFunnel->convertEOL = kNuConvertOff;
            }
            /* put it where the progress meter can see it */
            if (pFunnel->pProgress != nil)
                pFunnel->pProgress->expand.convertEOL = pFunnel->convertEOL;
        } else if (pFunnel->convertEOL == kNuConvertOn) {
            if (pFunnel->checkStripHighASCII) {
                /* assume this part of the buffer is representative */
                pFunnel->doStripHighASCII = Nu_CheckHighASCII(pFunnel,
                                                buffer, count);
            } else {
                Assert(!pFunnel->doStripHighASCII);
            }
            DBUG(("+++ Converter is on, convHighASCII=%d\n",
                pFunnel->doStripHighASCII));
        }
    }
    Assert(pFunnel->convertEOL != kNuConvertAuto);  /* on or off now */
    pFunnel->isFirstWrite = false;

    if (pFunnel->convertEOL == kNuConvertOff) {
        /* write it straight */
        Nu_FunnelPutBlock(pFunnel, buffer, count);
    } else {
        /* do the EOL conversion and optional high-bit stripping */
        Boolean lastCR = pFunnel->lastCR;   /* make local copy */
        uchar uch;
        int mask;

        if (pFunnel->doStripHighASCII)
            mask = 0x7f;
        else
            mask = 0xff;

        /*
         * We could get a significant speed improvement here by writing
         * non-EOL chars as a larger block instead of single bytes.
         */
        while (count--) {
            uch = (*buffer) & mask;

            if (uch == kNuCharCR) {
                Nu_PutEOL(pFunnel);
                lastCR = true;
            } else if (uch == kNuCharLF) {
                if (!lastCR)
                    Nu_PutEOL(pFunnel);
                lastCR = false;
            } else {
                Nu_FunnelPutBlock(pFunnel, &uch, 1);
                lastCR = false;
            }
            buffer++;
        }
        pFunnel->lastCR = lastCR;   /* save copy */

    }

    /*if (pFunnel->outMaxExceeded)
        err = kNuErrOutMax;*/

    err = Nu_DataSinkGetError(pFunnel->pDataSink);

    /* update progress counter with pre-LFCR count */
    if (err == kNuErrNone && pFunnel->pProgress != nil)
        pFunnel->pProgress->uncompressedProgress += progressCount;

    return err;
}


/*
 * Flush any data currently in the funnel.
 */
NuError
Nu_FunnelFlush(NuArchive* pArchive, NuFunnel* pFunnel)
{
    NuError err = kNuErrNone;

    if (!pFunnel->bufCount)
        goto bail;

    err = Nu_FunnelWriteConvert(pFunnel, pFunnel->buffer, pFunnel->bufCount);
    BailError(err);

    pFunnel->bufCount = 0;
    err = Nu_FunnelSendProgressUpdate(pArchive, pFunnel);
    /* fall through with error */

bail:
    return err;
}


/*
 * Write a bunch of bytes into a funnel.  They will be held in the buffer
 * if they fit, or flushed out the bottom if not.
 */
NuError
Nu_FunnelWrite(NuArchive* pArchive, NuFunnel* pFunnel, const uchar* buffer,
    ulong count)
{
    NuError err = kNuErrNone;

    /*pFunnel->inCount += count;*/

    /*
     * If it will fit into the buffer, just copy it in.
     */
    if (pFunnel->bufCount + count < kNuFunnelBufSize) {
        if (count == 1)     /* minor optimization */
            *(pFunnel->buffer + pFunnel->bufCount) = *buffer;
        else
            memcpy(pFunnel->buffer + pFunnel->bufCount, buffer, count);
        pFunnel->bufCount += count;
        goto bail;
    } else {
        /*
         * Won't fit.  We have to flush what we have, and we can either
         * blow out what we were just given or put it at the start of
         * the buffer.
         */
        if (pFunnel->bufCount) {
            err = Nu_FunnelFlush(pArchive, pFunnel);
            BailError(err);
        } else {
            err = Nu_FunnelSendProgressUpdate(pArchive, pFunnel);
            BailError(err);
        }

        Assert(pFunnel->bufCount == 0);

        if (count >= kNuFunnelBufSize / 4) {
            /* it's more than 25% of the buffer, just write it now */
            err = Nu_FunnelWriteConvert(pFunnel, buffer, count);
            BailError(err);
        } else {
            memcpy(pFunnel->buffer, buffer, count);
            pFunnel->bufCount = count;
        }
        goto bail;
    }

bail:
    return err;
}


/*
 * Set the Funnel's progress state.
 */
NuError
Nu_FunnelSetProgressState(NuFunnel* pFunnel, NuProgressState state)
{
    Assert(pFunnel != nil);

    if (pFunnel->pProgress == nil)
        return kNuErrNone;

    pFunnel->pProgress->state = state;

    return kNuErrNone;
}


/*
 * Send a progress update to the application, if they're interested.
 */
NuError
Nu_FunnelSendProgressUpdate(NuArchive* pArchive, NuFunnel* pFunnel)
{
    NuProgressData* pProgress;

    Assert(pArchive != nil);
    Assert(pFunnel != nil);

    pProgress = pFunnel->pProgress;
    if (pProgress == nil)
        return kNuErrNone;      /* no progress meter attached */

    /* don't continue if they're not accepting progress messages */
    if (pProgress->progressFunc == nil)
        return kNuErrNone;

    /* other than the choice of arguments, it's pretty much the same story */
    return Nu_SendInitialProgress(pArchive, pProgress);
}


/*
 * Pull the "doExpand" parameter out of the data source.
 */
Boolean
Nu_FunnelGetDoExpand(NuFunnel* pFunnel)
{
    Assert(pFunnel != nil);
    Assert(pFunnel->pDataSink != nil);

    return Nu_DataSinkGetDoExpand(pFunnel->pDataSink);
}


/*
 * ===========================================================================
 *      NuStraw object
 * ===========================================================================
 */

/*
 * Allocate and initialize a Straw.
 */
NuError
Nu_StrawNew(NuArchive* pArchive, NuDataSource* pDataSource,
    NuProgressData* pProgress, NuStraw** ppStraw)
{
    NuError err = kNuErrNone;
    NuStraw* pStraw = nil;

    Assert(ppStraw != nil);
    Assert(pDataSource != nil);

    pStraw = Nu_Calloc(pArchive, sizeof(*pStraw));
    BailAlloc(pStraw);
    pStraw->pDataSource = pDataSource;
    pStraw->pProgress = pProgress;
    pStraw->lastProgress = 0;
    pStraw->lastDisplayed = 0;

bail:
    if (err != kNuErrNone)
        Nu_StrawFree(pArchive, pStraw);
    else
        *ppStraw = pStraw;
    return err;
}

/*
 * Free a Straw.
 */
NuError
Nu_StrawFree(NuArchive* pArchive, NuStraw* pStraw)
{
    if (pStraw == nil)
        return kNuErrNone;

    /* we don't own the data source or progress meter */
    Nu_Free(pArchive, pStraw);

    return kNuErrNone;
}


/*
 * Set the Straw's progress state.
 */
NuError
Nu_StrawSetProgressState(NuStraw* pStraw, NuProgressState state)
{
    Assert(pStraw != nil);
    Assert(pStraw->pProgress != nil);

    pStraw->pProgress->state = state;

    return kNuErrNone;
}

/*
 * Send a progress update to the application, if they're interested.
 */
NuError
Nu_StrawSendProgressUpdate(NuArchive* pArchive, NuStraw* pStraw)
{
    NuProgressData* pProgress;

    Assert(pArchive != nil);
    Assert(pStraw != nil);

    pProgress = pStraw->pProgress;
    if (pProgress == nil)
        return kNuErrNone;      /* no progress meter attached */

    /* don't continue if they're not accepting progress messages */
    if (pProgress->progressFunc == nil)
        return kNuErrNone;

    /* other than the choice of arguments, it's pretty much the same story */
    return Nu_SendInitialProgress(pArchive, pProgress);
}


/*
 * Read data from a straw.
 */
NuError
Nu_StrawRead(NuArchive* pArchive, NuStraw* pStraw, uchar* buffer, long len)
{
    NuError err;

    Assert(pArchive != nil);
    Assert(pStraw != nil);
    Assert(buffer != nil);
    Assert(len > 0);

    /*
     * No buffering going on, so this is straightforward.
     */

    err = Nu_DataSourceGetBlock(pStraw->pDataSource, buffer, len);
    BailError(err);

    /*
     * Progress updating for adding is a little more complicated than
     * for extracting.  When extracting, the funnel controls the size
     * of the output buffer, and only pushes an update when the output
     * buffer fills.  Here, we don't know how much will be asked for at
     * a time, so we have to pace the updates or we risk flooding the
     * application.
     *
     * We also have another problem: we want to indicate how much data
     * has been processed, not how much data is *about* to be processed.
     * So we have to set the percentage based on how much was requested
     * on the previous call.  (This assumes that whatever they asked for
     * last time has already been fully processed.)
     */
    if (pStraw->pProgress != nil) {
        pStraw->pProgress->uncompressedProgress = pStraw->lastProgress;
        pStraw->lastProgress += len;

        if (!pStraw->pProgress->uncompressedProgress ||
            (pStraw->pProgress->uncompressedProgress - pStraw->lastDisplayed
                > (kNuFunnelBufSize * 3 / 4)))
        {
            err = Nu_StrawSendProgressUpdate(pArchive, pStraw);
            pStraw->lastDisplayed = pStraw->pProgress->uncompressedProgress;
            BailError(err);
        }

    }

bail:
    return err;
}


/*
 * Rewind a straw.  This rewinds the underlying data source, and resets
 * some progress counters.
 */
NuError
Nu_StrawRewind(NuArchive* pArchive, NuStraw* pStraw)
{
    Assert(pStraw != nil);
    Assert(pStraw->pDataSource != nil);

    pStraw->lastProgress = 0;
    pStraw->lastDisplayed = 0;

    return Nu_DataSourceRewind(pStraw->pDataSource);
}

