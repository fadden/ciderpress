/*
 * NuFX archive manipulation library
 * Copyright (C) 2000-2007 by Andy McFadden, All Rights Reserved.
 * This is free software; you can redistribute it and/or modify it under the
 * terms of the BSD License, see the file COPYING-LIB.
 *
 * Thread-level operations.
 */
#include "NufxLibPriv.h"


/*
 * ===========================================================================
 *      Utils
 * ===========================================================================
 */

/*
 * Returns thread N, or NULL if the index is invalid.
 */
NuThread* Nu_GetThread(const NuRecord* pRecord, int idx)
{
    if (idx >= (int)pRecord->recTotalThreads)
        return NULL;
    else
        return &pRecord->pThreads[idx];
}

/*
 * ShrinkIt v3.0.0 had a bug where the filename thread would get created
 * with the high bits set.  We want to undo that without stomping on
 * filenames that just happen to have a fancy character in them.  If all
 * of the high bits are set, assume it's a "defective" name and clear
 * them all.  If some aren't set, assume it's just a fancy filename.
 *
 * This high-bit-ism was also done for disk archives by most older versions
 * of ShrinkIt.
 */
void Nu_StripHiIfAllSet(char* str)
{
    uint8_t* cp;

    for (cp = (uint8_t*)str; *cp != '\0'; cp++)
        if (!(*cp & 0x80))
            return;

    for (cp = (uint8_t*)str; *cp != '\0'; cp++)
        *cp &= 0x7f;
}


/*
 * Decide if a thread is pre-sized (i.e. has a fixed maximum size with a
 * lesser amount of uncompressed data within) based on the threadID.
 */
Boolean Nu_IsPresizedThreadID(NuThreadID threadID)
{
    if (threadID == kNuThreadIDFilename || threadID == kNuThreadIDComment)
        return true;
    else
        return false;
}


/*
 * Return an indication of whether the type of thread specified by ThreadID
 * should ever be compressed.  Right now, that's only data-class threads.
 */
Boolean Nu_IsCompressibleThreadID(NuThreadID threadID)
{
    if (NuThreadIDGetClass(threadID) == kNuThreadClassData)
        return true;
    else
        return false;
}


/*
 * Decide if the thread has a CRC, based on the record version and the
 * threadID.
 */
Boolean Nu_ThreadHasCRC(uint16_t recordVersion, NuThreadID threadID)
{
    return recordVersion >= 3 &&
            NuThreadIDGetClass(threadID) == kNuThreadClassData;
}


/*
 * Search through a given NuRecord for the specified thread.
 */
NuError Nu_FindThreadByIdx(const NuRecord* pRecord, NuThreadIdx thread,
    NuThread** ppThread)
{
    NuThread* pThread;
    int idx;

    for (idx = 0; idx < (int)pRecord->recTotalThreads; idx++) {
        pThread = Nu_GetThread(pRecord, idx);
        Assert(pThread != NULL);

        if (pThread->threadIdx == thread) {
            *ppThread = pThread;
            return kNuErrNone;
        }
    }

    return kNuErrThreadIdxNotFound;
}


/*
 * Search through a given NuRecord for the first thread with a matching
 * threadID.
 */
NuError Nu_FindThreadByID(const NuRecord* pRecord, NuThreadID threadID,
    NuThread** ppThread)
{
    NuThread* pThread;
    int idx;

    for (idx = 0; idx < (int)pRecord->recTotalThreads; idx++) {
        pThread = Nu_GetThread(pRecord, idx);
        Assert(pThread != NULL);

        if (NuGetThreadID(pThread) == threadID) {
            *ppThread = pThread;
            return kNuErrNone;
        }
    }

    return kNuErrThreadIDNotFound;
}


/*
 * Copy the contents of a NuThread.
 */
void Nu_CopyThreadContents(NuThread* pDstThread, const NuThread* pSrcThread)
{
    Assert(pDstThread != NULL);
    Assert(pSrcThread != NULL);

    memcpy(pDstThread, pSrcThread, sizeof(*pDstThread));
}


/*
 * ===========================================================================
 *      Reading threads from the archive
 * ===========================================================================
 */

/*
 * Read a single thread header from the archive.
 */
static NuError Nu_ReadThreadHeader(NuArchive* pArchive, NuThread* pThread,
    uint16_t* pCrc)
{
    FILE* fp;

    Assert(pArchive != NULL);
    Assert(pThread != NULL);
    Assert(pCrc != NULL);

    fp = pArchive->archiveFp;

    pThread->thThreadClass = Nu_ReadTwoC(pArchive, fp, pCrc);
    pThread->thThreadFormat = Nu_ReadTwoC(pArchive, fp, pCrc);
    pThread->thThreadKind = Nu_ReadTwoC(pArchive, fp, pCrc);
    pThread->thThreadCRC = Nu_ReadTwoC(pArchive, fp, pCrc);
    pThread->thThreadEOF = Nu_ReadFourC(pArchive, fp, pCrc);
    pThread->thCompThreadEOF = Nu_ReadFourC(pArchive, fp, pCrc);

    pThread->threadIdx = Nu_GetNextThreadIdx(pArchive);
    pThread->actualThreadEOF = 0;   /* fix me later */
    pThread->fileOffset = -1;       /* mark as invalid */
    pThread->used = 0xcfcf;         /* init to invalid value */

    return Nu_HeaderIOFailed(pArchive, fp);
}

/*
 * Read the threads from the current archive file position.
 *
 * The storage for the threads is allocated here, in one block.  We could
 * have used a linked list like NuLib, but that doesn't really provide any
 * benefit for us, and adds complexity.
 */
NuError Nu_ReadThreadHeaders(NuArchive* pArchive, NuRecord* pRecord,
    uint16_t* pCrc)
{
    NuError err = kNuErrNone;
    NuThread* pThread;
    long count;
    Boolean needFakeData, needFakeRsrc;

    needFakeData = true;
    needFakeRsrc = (pRecord->recStorageType == kNuStorageExtended);

    Assert(pArchive != NULL);
    Assert(pRecord != NULL);
    Assert(pCrc != NULL);

    if (!pRecord->recTotalThreads) {
        /* not sure if this is reasonable, but we can handle it */
        DBUG(("--- WEIRD: no threads in the record?\n"));
        goto bail;
    }

    pRecord->pThreads = Nu_Malloc(pArchive,
                            pRecord->recTotalThreads * sizeof(NuThread));
    BailAlloc(pRecord->pThreads);

    count = pRecord->recTotalThreads;
    pThread = pRecord->pThreads;
    while (count--) {
        err = Nu_ReadThreadHeader(pArchive, pThread, pCrc);
        BailError(err);

        if (pThread->thThreadClass == kNuThreadClassData) {
            if (pThread->thThreadKind == kNuThreadKindDataFork) {
                needFakeData = false;
            } else if (pThread->thThreadKind == kNuThreadKindRsrcFork) {
                needFakeRsrc = false;
            }
        }

        /*
         * Some versions of ShrinkIt write an invalid thThreadEOF for disks,
         * so we have to figure out what it's supposed to be.
         */
        if (NuMakeThreadID(pThread->thThreadClass, pThread->thThreadKind) ==
            kNuThreadIDDiskImage)
        {
            if (pRecord->recStorageType <= 13) {
                /* supposed to be block size, but SHK v3.0.1 stored it wrong */
                pThread->actualThreadEOF = pRecord->recExtraType * 512;

            } else if (pRecord->recStorageType == 256 &&
                       pRecord->recExtraType == 280 &&
                       pRecord->recFileSysID == kNuFileSysDOS33)
            {
                /*
                 * Fix for less-common ShrinkIt problem: looks like an old
                 * version of GS/ShrinkIt used 256 as the block size when
                 * compressing DOS 3.3 images from 5.25" disks.  If that
                 * appears to be the case here, crank up the block size.
                 */
                DBUG(("--- no such thing as a 70K disk image!\n"));
                pThread->actualThreadEOF = pRecord->recExtraType * 512;

            } else {
                pThread->actualThreadEOF =
                    pRecord->recExtraType * pRecord->recStorageType;
            }
        } else {
            pThread->actualThreadEOF = pThread->thThreadEOF;
        }

        pThread->used = false;
        pThread++;
    }

    /*
     * If "mask threadless" is set, create "fake" threads with empty
     * data and resource forks as needed.
     */
    if ((needFakeData || needFakeRsrc) && pArchive->valMaskDataless) {
        int firstNewThread = pRecord->recTotalThreads;

        if (needFakeData) {
            pRecord->recTotalThreads++;
            pRecord->fakeThreads++;
        }
        if (needFakeRsrc) {
            pRecord->recTotalThreads++;
            pRecord->fakeThreads++;
        }

        pRecord->pThreads = Nu_Realloc(pArchive, pRecord->pThreads,
                                pRecord->recTotalThreads * sizeof(NuThread));
        BailAlloc(pRecord->pThreads);

        pThread = pRecord->pThreads + firstNewThread;

        if (needFakeData) {
            pThread->thThreadClass = kNuThreadClassData;
            pThread->thThreadFormat = kNuThreadFormatUncompressed;
            pThread->thThreadKind = kNuThreadKindDataFork;
            pThread->thThreadCRC = kNuInitialThreadCRC;
            pThread->thThreadEOF = 0;
            pThread->thCompThreadEOF = 0;
            pThread->threadIdx = Nu_GetNextThreadIdx(pArchive);
            pThread->actualThreadEOF = 0;
            pThread->fileOffset = -99999999;
            pThread->used = false;
            pThread++;
        }
        if (needFakeRsrc) {
            pThread->thThreadClass = kNuThreadClassData;
            pThread->thThreadFormat = kNuThreadFormatUncompressed;
            pThread->thThreadKind = kNuThreadKindRsrcFork;
            pThread->thThreadCRC = kNuInitialThreadCRC;
            pThread->thThreadEOF = 0;
            pThread->thCompThreadEOF = 0;
            pThread->threadIdx = Nu_GetNextThreadIdx(pArchive);
            pThread->actualThreadEOF = 0;
            pThread->fileOffset = -99999999;
            pThread->used = false;
        }
    }

bail:
    return err;
}


/*
 * Write a single thread header to the archive.
 */
static NuError Nu_WriteThreadHeader(NuArchive* pArchive,
    const NuThread* pThread, FILE* fp, uint16_t* pCrc)
{
    Assert(pArchive != NULL);
    Assert(pThread != NULL);
    Assert(fp != NULL);
    Assert(pCrc != NULL);

    Nu_WriteTwoC(pArchive, fp, pThread->thThreadClass, pCrc);
    Nu_WriteTwoC(pArchive, fp, (uint16_t)pThread->thThreadFormat, pCrc);
    Nu_WriteTwoC(pArchive, fp, pThread->thThreadKind, pCrc);
    Nu_WriteTwoC(pArchive, fp, pThread->thThreadCRC, pCrc);
    Nu_WriteFourC(pArchive, fp, pThread->thThreadEOF, pCrc);
    Nu_WriteFourC(pArchive, fp, pThread->thCompThreadEOF, pCrc);

    return Nu_HeaderIOFailed(pArchive, fp);
}

/*
 * Write the thread headers for the record at the current file position.
 *
 * Note this doesn't care whether a thread was "fake" or not.  In
 * effect, we promote all threads to "real" status.  We update the
 * "fake" count in pRecord accordingly.
 */
NuError Nu_WriteThreadHeaders(NuArchive* pArchive, NuRecord* pRecord, FILE* fp,
    uint16_t* pCrc)
{
    NuError err = kNuErrNone;
    NuThread* pThread;
    int idx;

    for (idx = 0; idx < (int)pRecord->recTotalThreads; idx++) {
        pThread = Nu_GetThread(pRecord, idx);
        Assert(pThread != NULL);

        err = Nu_WriteThreadHeader(pArchive, pThread, fp, pCrc);
        BailError(err);
    }

    if (pRecord->fakeThreads != 0) {
        DBUG(("+++ promoting %ld fake threads to real\n",pRecord->fakeThreads));
        pRecord->fakeThreads = 0;
    }

bail:
    return err;
}


/*
 * Compute miscellaneous thread information, like total size and file
 * offsets.  Some values (like file offsets) will not be useful for
 * streaming archives.
 *
 * Requires that the pArchive->currentOffset be set to the offset
 * immediately after the last of the thread headers.
 */
NuError Nu_ComputeThreadData(NuArchive* pArchive, NuRecord* pRecord)
{
    NuThread* pThread;
    long fileOffset, count;

    Assert(pArchive != NULL);
    Assert(pRecord != NULL);

    /*pRecord->totalLength = 0;*/
    pRecord->totalCompLength = 0;

    fileOffset = pArchive->currentOffset;

    count = pRecord->recTotalThreads;
    pThread = pRecord->pThreads;
    while (count--) {
        pThread->fileOffset = fileOffset;

        /*pRecord->totalLength += pThread->thThreadEOF;*/
        pRecord->totalCompLength += pThread->thCompThreadEOF;
        fileOffset += pThread->thCompThreadEOF;

        pThread++;
    }

    return kNuErrNone;
}


/*
 * Skip past some or all of the thread data in the archive.  For file
 * archives, we scan all the threads, but for streaming archives we only
 * want to scan up to the filename thread.  (If the filename thread comes
 * after one of the data threads, we have a problem!)
 *
 * The tricky part here is that we don't want to skip over a filename
 * thread.  We actually want to read it in, so that we have something to
 * show to the application.  (Someday I'll get AndyN for putting me
 * through this...)
 */
NuError Nu_ScanThreads(NuArchive* pArchive, NuRecord* pRecord, long numThreads)
{
    NuError err = kNuErrNone;
    NuThread* pThread;
    FILE* fp;

    Assert(pArchive != NULL);
    Assert(pRecord != NULL);

    fp = pArchive->archiveFp;

    Assert(numThreads <= (long)pRecord->recTotalThreads);

    pThread = pRecord->pThreads;
    while (numThreads--) {
        if (pRecord->threadFilenameMOR == NULL &&
            NuMakeThreadID(pThread->thThreadClass, pThread->thThreadKind) ==
                kNuThreadIDFilename)
        {
            /* it's the first filename thread, read the whole thing */
            if (pThread->thCompThreadEOF > kNuReasonableFilenameLen) {
                err = kNuErrBadRecord;
                Nu_ReportError(NU_BLOB, err, "Bad thread filename len (%u)",
                    pThread->thCompThreadEOF);
                goto bail;
            }
            pRecord->threadFilenameMOR = Nu_Malloc(pArchive,
                                        pThread->thCompThreadEOF +1);
            BailAlloc(pRecord->threadFilenameMOR);

            /* note there is no CRC on a filename thread */
            (void) Nu_ReadBytes(pArchive, fp, pRecord->threadFilenameMOR,
                    pThread->thCompThreadEOF);
            if ((err = Nu_HeaderIOFailed(pArchive, fp)) != kNuErrNone) {
                Nu_ReportError(NU_BLOB, err, "Failed reading filename thread");
                goto bail;
            }

            /* null-terminate on the actual len, not the buffer len */
            pRecord->threadFilenameMOR[pThread->thThreadEOF] = '\0';

            Nu_StripHiIfAllSet(pRecord->threadFilenameMOR);

            /* prefer this one over the record one, but only one should exist */
            if (pRecord->filenameMOR != NULL) {
                DBUG(("--- HEY: got record filename and thread filename\n"));
            }
            pRecord->filenameMOR = pRecord->threadFilenameMOR;

        } else {
            /* not a filename (or not first filename), skip past it */
            err = Nu_SeekArchive(pArchive, pArchive->archiveFp,
                    pThread->thCompThreadEOF, SEEK_CUR);
            BailError(err);
        }

        pThread++;
    }

    /*
     * Should've had one by now.  Supposedly, older versions of ShrinkIt
     * wouldn't prompt for a disk image name on DOS 3.3 volumes, so you'd
     * end up with a disk image that had no name attached.  This will tend
     * to confuse things, so we go ahead and give it a name.
     */
    if (pRecord->filenameMOR == NULL) {
        DBUG(("+++ no filename found, using default record name\n"));
        pRecord->filenameMOR = kNuDefaultRecordName;
    }

    pArchive->currentOffset += pRecord->totalCompLength;

    if (!Nu_IsStreaming(pArchive)) {
        Assert(pArchive->currentOffset == ftell(pArchive->archiveFp));
    }

bail:
    return err;
}


/*
 * Skip the thread.  This only has meaning for streaming archives, and
 * assumes that the file pointer is set to the start of the thread's data
 * already.
 */
NuError Nu_SkipThread(NuArchive* pArchive, const NuRecord* pRecord,
    const NuThread* pThread)
{
    NuError err;

    if (!Nu_IsStreaming(pArchive))      /* for debugging */
        return kNuErrNone;              /* for debugging */
    Assert(Nu_IsStreaming(pArchive));

    err = Nu_SeekArchive(pArchive, pArchive->archiveFp,
            pThread->thCompThreadEOF, SEEK_CUR);
    return err;
}


/*
 * ===========================================================================
 *      Extract
 * ===========================================================================
 */

/*
 * Extract the thread to the specified file pointer.
 *
 * If the archive is a stream, the stream must be positioned at the
 * start of pThread's data.  If not, it will be seeked first.
 */
static NuError Nu_ExtractThreadToDataSink(NuArchive* pArchive,
    const NuRecord* pRecord, const NuThread* pThread,
    NuProgressData* pProgress, NuDataSink* pDataSink)
{
    NuError err;
    NuFunnel* pFunnel = NULL;

    /* if it's not a stream, seek to the appropriate spot in the file */
    if (!Nu_IsStreaming(pArchive)) {
        err = Nu_SeekArchive(pArchive, pArchive->archiveFp,
                pThread->fileOffset, SEEK_SET);
        if (err != kNuErrNone) {
            Nu_ReportError(NU_BLOB, err, "Unable to seek input to %ld",
                pThread->fileOffset);
            goto bail;
        }
    }

    /*
     * Set up an output funnel to write to.
     */
    err = Nu_FunnelNew(pArchive, pDataSink, Nu_DataSinkGetConvertEOL(pDataSink),
            pArchive->valEOL, pProgress, &pFunnel);
    BailError(err);

    /*
     * Write it.
     */
    err = Nu_ExpandStream(pArchive, pRecord, pThread, pArchive->archiveFp,
            pFunnel);
    if (err != kNuErrNone) {
        if (err != kNuErrSkipped && err != kNuErrAborted)
            Nu_ReportError(NU_BLOB, err, "ExpandStream failed");
        goto bail;
    }

bail:
    (void) Nu_FunnelFree(pArchive, pFunnel);
    return err;
}


/*
 * Extract the specified thread to "pDataSink".  If the sink is to a file,
 * this will take care of opening (and, if appropriate, creating) the file.
 *
 * If we're operating on a streaming archive, the file pointer must be
 * positioned at the start of the thread's data.  If not, it will be
 * seeked appropriately.
 *
 * This calls the "should we extract" and "what pathname should we use"
 * filters for every thread, which means we can reject specific kinds
 * of forks and/or give them different names.  This is a good thing.
 */
static NuError Nu_ExtractThreadCommon(NuArchive* pArchive,
    const NuRecord* pRecord, const NuThread* pThread, NuDataSink* pDataSink)
{
    NuError err = kNuErrNone;
    NuSelectionProposal selProposal;
    NuPathnameProposal pathProposal;
    NuProgressData progressData;
    NuProgressData* pProgressData;
    NuDataSink* pOrigDataSink;
    UNICHAR* newPathStorageUNI = NULL;
    UNICHAR* recFilenameStorageUNI = NULL;
    const UNICHAR* newPathnameUNI;
    NuResult result;
    uint8_t newFssep;
    Boolean doFreeSink = false;

    Assert(pRecord != NULL);
    Assert(pThread != NULL);
    Assert(pDataSink != NULL);

    memset(&progressData, 0, sizeof(progressData));
    pProgressData = NULL;

    /*
     * If we're just trying to verify the archive contents, create a
     * data sink that goes nowhere at all.
     */
    if (pArchive->testMode) {
        err = Nu_DataSinkVoid_New(
                Nu_DataSinkGetDoExpand(pDataSink),
                Nu_DataSinkGetConvertEOL(pDataSink),
                &pDataSink);
        BailError(err);
        doFreeSink = true;
    }

    pOrigDataSink = pDataSink;  /* save a copy for the "retry" loop */

    /*
     * Decide if we want to extract this thread.  This is mostly for
     * use by the "bulk" extract, not the per-thread extract, but it
     * still applies if they so desire.
     */
    if (pArchive->selectionFilterFunc != NULL) {
        selProposal.pRecord = pRecord;
        selProposal.pThread = pThread;
        result = (*pArchive->selectionFilterFunc)(pArchive, &selProposal);

        if (result == kNuSkip)
            return Nu_SkipThread(pArchive, pRecord, pThread);
        if (result == kNuAbort) {
            err = kNuErrAborted;
            goto bail;
        }
    }

    newPathnameUNI = NULL;
    newFssep = 0;

    recFilenameStorageUNI = Nu_CopyMORToUNI(pRecord->filenameMOR);

retry_name:
    if (Nu_DataSinkGetType(pDataSink) == kNuDataSinkToFile) {
        /*
         * We're extracting.  Figure out the name of the file to write it to.
         * If they want to use the sleazy FILE* back door, create a new
         * data sink and use that instead.
         *
         * Start by resetting everything to defaults, in case this isn't
         * our first time through the "rename" loop.
         */
        newPathnameUNI = Nu_DataSinkFile_GetPathname(pDataSink);
        newFssep = Nu_DataSinkFile_GetFssep(pDataSink);
        pDataSink = pOrigDataSink;

        /* if they don't have a pathname func defined, we just use default */
        if (pArchive->outputPathnameFunc != NULL) {
            pathProposal.pathnameUNI = recFilenameStorageUNI;
            pathProposal.filenameSeparator =
                                NuGetSepFromSysInfo(pRecord->recFileSysInfo);
            pathProposal.pRecord = pRecord;
            pathProposal.pThread = pThread;
            pathProposal.newPathnameUNI = NULL;
            pathProposal.newFilenameSeparator = '\0';
            /*pathProposal.newStorage = (NuThreadID)-1;*/
            pathProposal.newDataSink = NULL;

            result = (*pArchive->outputPathnameFunc)(pArchive, &pathProposal);

            if (result == kNuSkip)
                return Nu_SkipThread(pArchive, pRecord, pThread);
            if (result == kNuAbort) {
                err = kNuErrAborted;
                goto bail;
            }

            /* we don't own this string, so make a copy */
            if (pathProposal.newPathnameUNI != NULL) {
                Nu_Free(pArchive, newPathStorageUNI);
                newPathStorageUNI = strdup(pathProposal.newPathnameUNI);
                newPathnameUNI = newPathStorageUNI;
            } else {
                newPathnameUNI = NULL;
            }
            if (pathProposal.newFilenameSeparator != '\0')
                newFssep = pathProposal.newFilenameSeparator;

            /* if they want to send this somewhere else, let them */
            if (pathProposal.newDataSink != NULL)
                pDataSink = pathProposal.newDataSink;
        }

        /* at least one of these must be set */
        Assert(!(newPathnameUNI == NULL && pathProposal.newDataSink == NULL));
    }

    /*
     * Prepare the progress data if this is a data thread.
     */
    if (newPathnameUNI == NULL) {
        /* using a data sink; get the pathname out of the record */
        newPathnameUNI = recFilenameStorageUNI;
        newFssep = NuGetSepFromSysInfo(pRecord->recFileSysInfo);
    }
    if (pThread->thThreadClass == kNuThreadClassData) {
        pProgressData = &progressData;
        err = Nu_ProgressDataInit_Expand(pArchive, pProgressData, pRecord,
                newPathnameUNI, newFssep, recFilenameStorageUNI,
                Nu_DataSinkGetConvertEOL(pOrigDataSink));
        BailError(err);

        /* send initial progress so they see the right name if "open" fails */
        pProgressData->state = kNuProgressOpening;
        err = Nu_SendInitialProgress(pArchive, pProgressData);
        BailError(err);
    }

    if (Nu_DataSinkGetType(pDataSink) == kNuDataSinkToFile) {
        /*
         * We're extracting to a file.  Open it, creating it if necessary and
         * allowed.
         */
        FILE* fileFp = NULL;

        err = Nu_OpenOutputFile(pArchive, pRecord, pThread, newPathnameUNI,
                newFssep, &fileFp);
        if (err == kNuErrRename) {
            /* they want to rename; the OutputPathname callback handles this */
            Nu_Free(pArchive, newPathStorageUNI);
            newPathStorageUNI = NULL;
            /* reset these just to be careful */
            newPathnameUNI = NULL;
            fileFp = NULL;
            goto retry_name;
        } else if (err != kNuErrNone) {
            goto bail;
        }

        Assert(fileFp != NULL);
        (void) Nu_DataSinkFile_SetFP(pDataSink, fileFp);

        DBUG(("+++ EXTRACTING 0x%08lx from '%s' at offset %0ld to '%s'\n",
            NuMakeThreadID(pThread->thThreadClass, pThread->thThreadKind),
            pRecord->filename, pThread->fileOffset, newPathname));
    } else {
        DBUG(("+++ EXTRACTING 0x%08lx from '%s' at offset %0ld to sink\n",
            NuMakeThreadID(pThread->thThreadClass, pThread->thThreadKind),
            pRecord->filename, pThread->fileOffset));
    }

    /* extract to the file */
    err = Nu_ExtractThreadToDataSink(pArchive, pRecord, pThread,
            pProgressData, pDataSink);
    BailError(err);

    if (Nu_DataSinkGetType(pDataSink) == kNuDataSinkToFile) {
        /*
         * Close the file, adjusting the modification date and access
         * permissions as appropriate.
         */
        err = Nu_CloseOutputFile(pArchive, pRecord,
                Nu_DataSinkFile_GetFP(pDataSink), newPathnameUNI);
        Nu_DataSinkFile_SetFP(pDataSink, NULL);
        BailError(err);
    }

bail:
    if (err != kNuErrNone && pProgressData != NULL) {
        /* send a final progress message, indicating failure */
        if (err == kNuErrSkipped)
            pProgressData->state = kNuProgressSkipped;
        else if (err == kNuErrAborted)
            pProgressData->state = kNuProgressAborted;
        else
            pProgressData->state = kNuProgressFailed;
        (void) Nu_SendInitialProgress(pArchive, pProgressData);
    }

    /* if this was an ordinary file, and it's still open, close it */
    if (Nu_DataSinkGetType(pDataSink) == kNuDataSinkToFile)
        Nu_DataSinkFile_Close(pDataSink);

    Nu_Free(pArchive, newPathStorageUNI);
    Nu_Free(pArchive, recFilenameStorageUNI);

    if (doFreeSink)
        Nu_DataSinkFree(pDataSink);
    return err;
}

/*
 * Extract a thread from the archive as part of a "bulk" extract operation.
 *
 * Streaming archives must be properly positioned.
 */
NuError Nu_ExtractThreadBulk(NuArchive* pArchive, const NuRecord* pRecord,
    const NuThread* pThread)
{
    NuError err;
    NuDataSink* pDataSink = NULL;
    UNICHAR* recFilenameStorageUNI = NULL;
    NuValue eolConv;

    /*
     * Create a file data sink for the file.  We use whatever EOL conversion
     * is set as the default for the entire archive.  (If you want to
     * specify your own EOL conversion for each individual file, you will
     * need to extract them individually, creating a data sink for each.)
     *
     * One exception: we turn EOL conversion off for disk image threads.
     * It's *very* unlikely this would be desirable, and could be a problem
     * if the user is extracting a collection of disks and files.
     */
    eolConv = pArchive->valConvertExtractedEOL;
    if (NuGetThreadID(pThread) == kNuThreadIDDiskImage)
        eolConv = kNuConvertOff;
    recFilenameStorageUNI = Nu_CopyMORToUNI(pRecord->filenameMOR);
    err = Nu_DataSinkFile_New(true, eolConv, recFilenameStorageUNI,
            NuGetSepFromSysInfo(pRecord->recFileSysInfo), &pDataSink);
    BailError(err);

    err = Nu_ExtractThreadCommon(pArchive, pRecord, pThread, pDataSink);
    BailError(err);

bail:
    if (pDataSink != NULL) {
        NuError err2 = Nu_DataSinkFree(pDataSink);
        if (err == kNuErrNone)
            err = err2;
    }
    Nu_Free(pArchive, recFilenameStorageUNI);

    return err;
}


/*
 * Extract a thread, given the IDs and a data sink.
 */
NuError Nu_ExtractThread(NuArchive* pArchive, NuThreadIdx threadIdx,
    NuDataSink* pDataSink)
{
    NuError err;
    NuRecord* pRecord;
    NuThread* pThread;

    if (Nu_IsStreaming(pArchive))
        return kNuErrUsage;
    if (threadIdx == 0 || pDataSink == NULL)
        return kNuErrInvalidArg;
    err = Nu_GetTOCIfNeeded(pArchive);
    BailError(err);

    /* find the correct record and thread by index */
    err = Nu_RecordSet_FindByThreadIdx(&pArchive->origRecordSet, threadIdx,
            &pRecord, &pThread);
    BailError(err);
    Assert(pRecord != NULL);

    /* extract away */
    err = Nu_ExtractThreadCommon(pArchive, pRecord, pThread, pDataSink);
    BailError(err);

bail:
    return err;
}


/*
 * ===========================================================================
 *      Add/update/delete
 * ===========================================================================
 */

/*
 * Verify that a conflicting thread with the specified threadID does not
 * exist in this record, now or in the future.
 *
 * The set of interesting threads is equal to the current threads, minus
 * any that have been deleted, plus any that have been added already.
 *
 * If a matching threadID is found, this returns an error.
 */
static NuError Nu_FindNoFutureThread(NuArchive* pArchive,
    const NuRecord* pRecord, NuThreadID threadID)
{
    NuError err = kNuErrNone;
    const NuThread* pThread;
    const NuThreadMod* pThreadMod;
    int idx;

    /*
     * Start by scanning the existing threads (if any).
     */
    for (idx = 0; idx < (int)pRecord->recTotalThreads; idx++) {
        pThread = Nu_GetThread(pRecord, idx);
        Assert(pThread != NULL);

        if (NuGetThreadID(pThread) == threadID) {
            /* found a match, see if it has been deleted */
            pThreadMod = Nu_ThreadMod_FindByThreadIdx(pRecord,
                            pThread->threadIdx);
            if (pThreadMod != NULL &&
                pThreadMod->entry.kind == kNuThreadModDelete)
            {
                /* it's deleted, ignore it */
                continue;
            }
            DBUG(("--- found existing thread matching 0x%08lx\n", threadID));
            err = kNuErrThreadAdd;
            goto bail;
        }
    }

    /*
     * Now look for "add" threadMods with a matching threadID.
     */
    pThreadMod = pRecord->pThreadMods;
    while (pThreadMod != NULL) {
        if (pThreadMod->entry.kind == kNuThreadModAdd &&
            pThreadMod->entry.add.threadID == threadID)
        {
            DBUG(("--- found 'add' threadMod matching 0x%08lx\n", threadID));
            err = kNuErrThreadAdd;
            goto bail;
        }

        pThreadMod = pThreadMod->pNext;
    }

bail:
    return err;
}

/*
 * Like Nu_FindNoFutureThread, but tests against a whole class.
 */
static NuError Nu_FindNoFutureThreadClass(NuArchive* pArchive,
    const NuRecord* pRecord, long threadClass)
{
    NuError err = kNuErrNone;
    const NuThread* pThread;
    const NuThreadMod* pThreadMod;
    int idx;

    /*
     * Start by scanning the existing threads (if any).
     */
    for (idx = 0; idx < (int)pRecord->recTotalThreads; idx++) {
        pThread = Nu_GetThread(pRecord, idx);
        Assert(pThread != NULL);

        if (pThread->thThreadClass == threadClass) {
            /* found a match, see if it has been deleted */
            pThreadMod = Nu_ThreadMod_FindByThreadIdx(pRecord,
                            pThread->threadIdx);
            if (pThreadMod != NULL &&
                pThreadMod->entry.kind == kNuThreadModDelete)
            {
                /* it's deleted, ignore it */
                continue;
            }
            DBUG(("--- Found existing thread matching 0x%04lx\n", threadClass));
            err = kNuErrThreadAdd;
            goto bail;
        }
    }

    /*
     * Now look for "add" threadMods with a matching threadClass.
     */
    pThreadMod = pRecord->pThreadMods;
    while (pThreadMod != NULL) {
        if (pThreadMod->entry.kind == kNuThreadModAdd &&
            NuThreadIDGetClass(pThreadMod->entry.add.threadID) == threadClass)
        {
            DBUG(("--- Found 'add' threadMod matching 0x%04lx\n", threadClass));
            err = kNuErrThreadAdd;
            goto bail;
        }

        pThreadMod = pThreadMod->pNext;
    }

bail:
    return err;
}


/*
 * Find an existing thread somewhere in the archive.  If the "copy" set
 * exists it will be searched.  If not, the "orig" set is searched, and
 * if an entry is found a "copy" set will be created.
 *
 * The record and thread returned will always be from the "copy" set.  An
 * error result is returned if the record and thread aren't found.
 */
static NuError Nu_FindThreadForWriteByIdx(NuArchive* pArchive,
    NuThreadIdx threadIdx, NuRecord** ppFoundRecord, NuThread** ppFoundThread)
{
    NuError err;

    if (Nu_RecordSet_GetLoaded(&pArchive->copyRecordSet)) {
        err = Nu_RecordSet_FindByThreadIdx(&pArchive->copyRecordSet, threadIdx,
                ppFoundRecord, ppFoundThread);
    } else {
        Assert(Nu_RecordSet_GetLoaded(&pArchive->origRecordSet));
        err = Nu_RecordSet_FindByThreadIdx(&pArchive->origRecordSet, threadIdx,
                ppFoundRecord, ppFoundThread);
        *ppFoundThread = NULL;       /* can't delete from here, wipe ptr */
    }
    BailError(err);

    /*
     * The thread exists.  If we were looking in the "orig" set, we have
     * to create a "copy" set, and delete it from that.
     */
    if (*ppFoundThread == NULL) {
        err = Nu_RecordSet_Clone(pArchive, &pArchive->copyRecordSet,
                &pArchive->origRecordSet);
        BailError(err);
        err = Nu_RecordSet_FindByThreadIdx(&pArchive->copyRecordSet, threadIdx,
                ppFoundRecord, ppFoundThread);
        Assert(err == kNuErrNone && *ppFoundThread != NULL); /* must succeed */
        BailError(err);
    }

bail:
    return err;
}

/*
 * Determine if it's okay to add a thread of the type specified by
 * "threadID" into "pRecord".
 *
 * Returns with an error (kNuErrThreadAdd) if it's not okay.
 */
NuError Nu_OkayToAddThread(NuArchive* pArchive, const NuRecord* pRecord,
    NuThreadID threadID)
{
    NuError err = kNuErrNone;

    /*
     * Check for class conflicts (can't mix data and control threads).
     */
    if (NuThreadIDGetClass(threadID) == kNuThreadClassData) {
        err = Nu_FindNoFutureThreadClass(pArchive, pRecord,
                kNuThreadClassControl);
        BailError(err);
    } else if (NuThreadIDGetClass(threadID) == kNuThreadClassControl) {
        err = Nu_FindNoFutureThreadClass(pArchive, pRecord,
                kNuThreadClassData);
        BailError(err);
    }

    /*
     * Check for specific type conflicts.
     */
    if (threadID == kNuThreadIDDataFork) {
        err = Nu_FindNoFutureThread(pArchive, pRecord, kNuThreadIDDataFork);
        BailError(err);
        err = Nu_FindNoFutureThread(pArchive, pRecord, kNuThreadIDDiskImage);
        BailError(err);
    } else if (threadID == kNuThreadIDRsrcFork) {
        err = Nu_FindNoFutureThread(pArchive, pRecord, kNuThreadIDRsrcFork);
        BailError(err);
        err = Nu_FindNoFutureThread(pArchive, pRecord, kNuThreadIDDiskImage);
        BailError(err);
    } else if (threadID == kNuThreadIDDiskImage) {
        err = Nu_FindNoFutureThread(pArchive, pRecord, kNuThreadIDDataFork);
        BailError(err);
        err = Nu_FindNoFutureThread(pArchive, pRecord, kNuThreadIDRsrcFork);
        BailError(err);
        err = Nu_FindNoFutureThread(pArchive, pRecord, kNuThreadIDDiskImage);
        BailError(err);
    } else if (threadID == kNuThreadIDFilename) {
        err = Nu_FindNoFutureThread(pArchive, pRecord, kNuThreadIDFilename);
        BailError(err);
    }

bail:
    return err;
}



/*
 * Add a new thread to a record.
 *
 * In some cases, you aren't allowed to add a thread whose type matches
 * one that already exists.  This applies to data threads and filenames,
 * but not to comments, control threads, or IIgs icons.  You also can't
 * add a disk image thread when there are data-class threads, or vice-versa.
 *
 * This is the first and last place we do this sort of checking.  If
 * an illegal situation gets past this function, it will either get
 * caught with a fatal assert or (if NDEBUG is defined) not at all.
 *
 * On success, the NuThreadIdx of the newly-created record will be placed
 * in "*pThreadIdx", and "pDataSource" will be owned by NufxLib.
 */
NuError Nu_AddThread(NuArchive* pArchive, NuRecordIdx recIdx,
    NuThreadID threadID, NuDataSource* pDataSource, NuThreadIdx* pThreadIdx)
{
    NuError err;
    NuRecord* pRecord;
    NuThreadMod* pThreadMod = NULL;
    NuThreadFormat threadFormat;

    /* okay for pThreadIdx to be NULL */
    if (recIdx == 0 || pDataSource == NULL)
        return kNuErrInvalidArg;

    if (Nu_IsReadOnly(pArchive))
        return kNuErrArchiveRO;
    err = Nu_GetTOCIfNeeded(pArchive);
    BailError(err);

    /*
     * Find the record.  If it doesn't exist in the copy set, check to
     * see if it's in the "new" set.
     */
    err = Nu_FindRecordForWriteByIdx(pArchive, recIdx, &pRecord);
    if (err == kNuErrRecIdxNotFound &&
        Nu_RecordSet_GetLoaded(&pArchive->newRecordSet))
    {
        err = Nu_RecordSet_FindByIdx(&pArchive->newRecordSet, recIdx, &pRecord);
    }
    BailError(err);
    Assert(pRecord != NULL);

    /*
     * Do some tests, looking for specific types of threads that conflict
     * with what we're trying to add.
     */
    err = Nu_OkayToAddThread(pArchive, pRecord, threadID);
    BailError(err);

    /*
     * Decide if we want to compress the data from this source.  If the
     * data is already compressed (as indicated by the data source) or
     * this type of thread isn't compressible (e.g. it's a filename), then
     * we don't compress it.  Otherwise, we use whatever compression mode
     * is currently configured.
     */
    if (Nu_DataSourceGetThreadFormat(pDataSource) == kNuThreadFormatUncompressed &&
        Nu_IsCompressibleThreadID(threadID))
    {
        threadFormat = Nu_ConvertCompressValToFormat(pArchive,
                        pArchive->valDataCompression);
    } else {
        threadFormat = kNuThreadFormatUncompressed;
    }
    DBUG(("--- using threadFormat = %d\n", threadFormat));

    /* create a new ThreadMod (which makes a copy of the data source) */
    err = Nu_ThreadModAdd_New(pArchive, threadID, threadFormat, pDataSource,
            &pThreadMod);
    BailError(err);
    Assert(pThreadMod != NULL);

    /* add the thread mod to the record */
    Nu_RecordAddThreadMod(pRecord, pThreadMod);
    if (pThreadIdx != NULL)
        *pThreadIdx = pThreadMod->entry.add.threadIdx;
    pThreadMod = NULL;   /* successful, don't free */

    /*
     * If we've got a header filename and we're adding a filename thread,
     * we don't want to write the record header name when we reconstruct
     * the record.
     */
    if (threadID == kNuThreadIDFilename && pRecord->recFilenameLength) {
        DBUG(("+++ gonna drop the filename\n"));
        pRecord->dropRecFilename = true;
    }

bail:
    if (pThreadMod != NULL)
        Nu_ThreadModFree(pArchive, pThreadMod);
    if (err == kNuErrNone && pDataSource != NULL) {
        /* on success, we have ownership of the data source.  ThreadMod
           made its own copy, so get rid of this one */
        Nu_DataSourceFree(pDataSource);
    }
    return err;
}


/*
 * Update the contents of a pre-sized thread, such as a filename or
 * comment thread.
 *
 * The data from the source must fit within the limits of the existing
 * thread.  The source data is never compressed, and must not come from
 * a compressed source.
 *
 * You aren't allowed to update threads that have been deleted.  Updating
 * newly-added threads isn't possible, since they aren't really threads yet.
 */
NuError Nu_UpdatePresizedThread(NuArchive* pArchive, NuThreadIdx threadIdx,
    NuDataSource* pDataSource, int32_t* pMaxLen)
{
    NuError err;
    NuThreadMod* pThreadMod = NULL;
    NuRecord* pFoundRecord;
    NuThread* pFoundThread;

    if (pDataSource == NULL) {
        err = kNuErrInvalidArg;
        goto bail;
    }

    /* presized threads always contain uncompressed data */
    if (Nu_DataSourceGetThreadFormat(pDataSource) !=
        kNuThreadFormatUncompressed)
    {
        err = kNuErrBadFormat;
        Nu_ReportError(NU_BLOB, err,
            "presized threads can't hold compressed data");
        goto bail;
    }

    if (Nu_IsReadOnly(pArchive))
        return kNuErrArchiveRO;
    err = Nu_GetTOCIfNeeded(pArchive);
    BailError(err);

    /*
     * Find the thread in the "copy" set.  (If there isn't a copy set,
     * make one.)
     */
    err = Nu_FindThreadForWriteByIdx(pArchive, threadIdx, &pFoundRecord,
            &pFoundThread);
    BailError(err);

    if (!Nu_IsPresizedThreadID(NuGetThreadID(pFoundThread)) ||
        !(pFoundThread->thCompThreadEOF >= pFoundThread->thThreadEOF))
    {
        err = kNuErrNotPreSized;
        Nu_ReportError(NU_BLOB, err, "invalid thread for update");
        goto bail;
    }

    if (pMaxLen != NULL)
        *pMaxLen = pFoundThread->thCompThreadEOF;

    /*
     * Check to see if somebody is trying to delete this, or has already
     * updated it.
     */
    if (Nu_ThreadMod_FindByThreadIdx(pFoundRecord, threadIdx) != NULL) {
        DBUG(("--- Tried to modify a deleted or modified thread\n"));
        err = kNuErrModThreadChange;
        goto bail;
    }

    /*
     * Verify that "otherLen" in the data source is less than or equal
     * to our len, if we can.  If the data source is a file on disk,
     * we're not really supposed to look at it until we flush.  We
     * could sneak a peek right now, which would prevent us from aborting
     * the entire operation when it turns out the file won't fit, but
     * that violates our semantics (and besides, the application really
     * should've done that already).
     *
     * If the data source is from a file, we just assume it'll fit and
     * let the chips fall where they may later on.
     */
    if (Nu_DataSourceGetType(pDataSource) != kNuDataSourceFromFile) {
        if (pFoundThread->thCompThreadEOF <
            Nu_DataSourceGetOtherLen(pDataSource))
        {
            err = kNuErrPreSizeOverflow;
            Nu_ReportError(NU_BLOB, err, "can't put %u bytes into %u",
                Nu_DataSourceGetOtherLen(pDataSource),
                pFoundThread->thCompThreadEOF);
            goto bail;
        }

        /* check for zero-length and excessively long filenames */
        if (NuGetThreadID(pFoundThread) == kNuThreadIDFilename &&
            (Nu_DataSourceGetOtherLen(pDataSource) == 0 ||
             Nu_DataSourceGetOtherLen(pDataSource) > kNuReasonableFilenameLen))
        {
            err = kNuErrInvalidFilename;
            Nu_ReportError(NU_BLOB, err, "invalid filename (%u bytes)",
                Nu_DataSourceGetOtherLen(pDataSource));
            goto bail;
        }
    }

    /*
     * Looks like it'll fit, and it's the right kind of data.  Create
     * an "update" threadMod.  Note this copies the data source.
     */
    Assert(pFoundThread->thThreadFormat == kNuThreadFormatUncompressed);
    err = Nu_ThreadModUpdate_New(pArchive, threadIdx, pDataSource, &pThreadMod);
    BailError(err);
    Assert(pThreadMod != NULL);

    /* add the thread mod to the record */
    Nu_RecordAddThreadMod(pFoundRecord, pThreadMod);

    /*
     * NOTE: changes to filename threads will be picked up later and
     * incorporated into the record's threadFilename.  We don't worry
     * about the record header filename, because we might be doing an
     * update-in-place and that prevents us from removing the filename
     * (doing so would change the size of the archive).  No need to
     * do any filename-specific changes here.
     */

bail:
    return err;
}

/*
 * Delete an individual thread.
 *
 * You aren't allowed to delete threads that have been updated.  Deleting
 * newly-added threads isn't possible, since they aren't really threads yet.
 *
 * Don't worry about deleting filename threads here; we take care of that
 * later on.  Besides, it's sort of handy to hang on to the filename for
 * as long as possible.
 */
NuError Nu_DeleteThread(NuArchive* pArchive, NuThreadIdx threadIdx)
{
    NuError err;
    NuThreadMod* pThreadMod = NULL;
    NuRecord* pFoundRecord;
    NuThread* pFoundThread;

    if (Nu_IsReadOnly(pArchive))
        return kNuErrArchiveRO;
    err = Nu_GetTOCIfNeeded(pArchive);
    BailError(err);

    /*
     * Find the thread in the "copy" set.  (If there isn't a copy set,
     * make one.)
     */
    err = Nu_FindThreadForWriteByIdx(pArchive, threadIdx, &pFoundRecord,
            &pFoundThread);
    BailError(err);

    /*
     * Deletion of modified threads (updates or previous deletes) isn't
     * allowed.  Deletion of threads from deleted records can't happen,
     * because deleted records are completely removed from the "copy" set.
     */
    if (Nu_ThreadMod_FindByThreadIdx(pFoundRecord, threadIdx) != NULL) {
        DBUG(("--- Tried to delete a deleted or modified thread\n"));
        err = kNuErrModThreadChange;
        goto bail;
    }

    /*
     * Looks good.  Add a new "delete" ThreadMod to the list.
     */
    err = Nu_ThreadModDelete_New(pArchive, threadIdx,
                NuGetThreadID(pFoundThread), &pThreadMod);
    BailError(err);
    Nu_RecordAddThreadMod(pFoundRecord, pThreadMod);
    pThreadMod = NULL;   /* successful, don't free */

bail:
    Nu_ThreadModFree(pArchive, pThreadMod);
    return err;
}

