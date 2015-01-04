/*
 * NuFX archive manipulation library
 * Copyright (C) 2000-2007 by Andy McFadden, All Rights Reserved.
 * This is free software; you can redistribute it and/or modify it under the
 * terms of the BSD License, see the file COPYING-LIB.
 *
 * Deferred write handling.
 */
#include "NufxLibPriv.h"


/*
 * ===========================================================================
 *      NuThreadMod functions
 * ===========================================================================
 */

/*
 * Alloc and initialize a new "add" ThreadMod.
 *
 * Caller is allowed to dispose of the data source, as this makes a copy.
 *
 * NOTE: threadFormat is how you want the data to be compressed.  The
 * threadFormat passed to DataSource describes the source data.
 */
NuError Nu_ThreadModAdd_New(NuArchive* pArchive, NuThreadID threadID,
    NuThreadFormat threadFormat, NuDataSource* pDataSource,
    NuThreadMod** ppThreadMod)
{
    Assert(ppThreadMod != NULL);
    Assert(pDataSource != NULL);

    *ppThreadMod = Nu_Calloc(pArchive, sizeof(**ppThreadMod));
    if (*ppThreadMod == NULL)
        return kNuErrMalloc;

    (*ppThreadMod)->entry.kind = kNuThreadModAdd;
    (*ppThreadMod)->entry.add.used = false;
    (*ppThreadMod)->entry.add.threadIdx = Nu_GetNextThreadIdx(pArchive);
    (*ppThreadMod)->entry.add.threadID = threadID;
    (*ppThreadMod)->entry.add.threadFormat = threadFormat;
    (*ppThreadMod)->entry.add.pDataSource = Nu_DataSourceCopy(pDataSource);

    /* decide if this is a pre-sized thread [do we want to do this here??] */
    (*ppThreadMod)->entry.add.isPresized = Nu_IsPresizedThreadID(threadID);

    return kNuErrNone;
}

/*
 * Alloc and initialize a new "update" ThreadMod.
 *
 * Caller is allowed to dispose of the data source.
 */
NuError Nu_ThreadModUpdate_New(NuArchive* pArchive, NuThreadIdx threadIdx,
    NuDataSource* pDataSource, NuThreadMod** ppThreadMod)
{
    Assert(ppThreadMod != NULL);
    Assert(pDataSource != NULL);

    *ppThreadMod = Nu_Calloc(pArchive, sizeof(**ppThreadMod));
    if (*ppThreadMod == NULL)
        return kNuErrMalloc;

    (*ppThreadMod)->entry.kind = kNuThreadModUpdate;
    (*ppThreadMod)->entry.update.used = false;
    (*ppThreadMod)->entry.update.threadIdx = threadIdx;
    (*ppThreadMod)->entry.update.pDataSource = Nu_DataSourceCopy(pDataSource);

    return kNuErrNone;
}

/*
 * Alloc and initialize a new "delete" ThreadMod.
 *
 * The "threadID" argument is really only needed for filename threads.  We
 * use it when trying to track how many filename threads we really have.
 */
NuError Nu_ThreadModDelete_New(NuArchive* pArchive, NuThreadIdx threadIdx,
    NuThreadID threadID, NuThreadMod** ppThreadMod)
{
    Assert(ppThreadMod != NULL);

    *ppThreadMod = Nu_Calloc(pArchive, sizeof(**ppThreadMod));
    if (*ppThreadMod == NULL)
        return kNuErrMalloc;

    (*ppThreadMod)->entry.kind = kNuThreadModDelete;
    (*ppThreadMod)->entry.delete.used = false;
    (*ppThreadMod)->entry.delete.threadIdx = threadIdx;
    (*ppThreadMod)->entry.delete.threadID = threadID;

    return kNuErrNone;
}

/*
 * Free a single NuThreadMod.
 */
void Nu_ThreadModFree(NuArchive* pArchive, NuThreadMod* pThreadMod)
{
    if (pThreadMod == NULL)
        return;

    switch (pThreadMod->entry.kind) {
    case kNuThreadModAdd:
        Nu_DataSourceFree(pThreadMod->entry.add.pDataSource);
        break;
    case kNuThreadModUpdate:
        Nu_DataSourceFree(pThreadMod->entry.update.pDataSource);
        break;
    default:
        break;
    }

    Nu_Free(pArchive, pThreadMod);
}


/*
 * Return a threadMod with a matching "threadIdx", if any.  Because "add"
 * threads can't have a threadIdx that matches an existing thread, this
 * will only return updates and deletes.
 *
 * We don't allow more than one threadMod on the same thread, so we don't
 * have to deal with having more than one match.  (To be safe, we go
 * ahead and do debug-only checks for multiple matches.  There shouldn't
 * be more than three or four threads per record, so the extra search
 * isn't costly.)
 *
 * Returns "NULL" if nothing found.
 */
NuThreadMod* Nu_ThreadMod_FindByThreadIdx(const NuRecord* pRecord,
    NuThreadIdx threadIdx)
{
    NuThreadMod* pThreadMod;
    NuThreadMod* pMatch = NULL;

    pThreadMod = pRecord->pThreadMods;
    while (pThreadMod) {
        switch (pThreadMod->entry.kind) {
        case kNuThreadModAdd:
            /* can't happen */
            Assert(pThreadMod->entry.add.threadIdx != threadIdx);
            break;
        case kNuThreadModUpdate:
            if (pThreadMod->entry.update.threadIdx == threadIdx) {
                Assert(pMatch == NULL);
                pMatch = pThreadMod;
            }
            break;
        case kNuThreadModDelete:
            if (pThreadMod->entry.delete.threadIdx == threadIdx) {
                Assert(pMatch == NULL);
                pMatch = pThreadMod;
            }
            break;
        default:
            Assert(0);
            /* keep going, I guess */
        }
        pThreadMod = pThreadMod->pNext;
    }

    return pMatch;
}


/*
 * ===========================================================================
 *      ThreadMod list operations
 * ===========================================================================
 */

/*
 * Search for an "add" ThreadMod, by threadID.
 */
NuError Nu_ThreadModAdd_FindByThreadID(const NuRecord* pRecord,
    NuThreadID threadID, NuThreadMod** ppThreadMod)
{
    NuThreadMod* pThreadMod;

    Assert(pRecord != NULL);
    Assert(ppThreadMod != NULL);

    pThreadMod = pRecord->pThreadMods;
    while (pThreadMod != NULL) {
        if (pThreadMod->entry.kind != kNuThreadModAdd)
            continue;

        if (pThreadMod->entry.add.threadID == threadID) {
            *ppThreadMod = pThreadMod;
            return kNuErrNone;
        }

        pThreadMod = pThreadMod->pNext;
    }

    return kNuErrNotFound;
}


/*
 * Free up the list of NuThreadMods in this record.
 */
void Nu_FreeThreadMods(NuArchive* pArchive, NuRecord* pRecord)
{
    NuThreadMod* pThreadMod;
    NuThreadMod* pNext;

    Assert(pRecord != NULL);
    pThreadMod = pRecord->pThreadMods;

    if (pThreadMod == NULL)
        return;

    while (pThreadMod != NULL) {
        pNext = pThreadMod->pNext;

        Nu_ThreadModFree(pArchive, pThreadMod);
        pThreadMod = pNext;
    }

    pRecord->pThreadMods = NULL;
}


/*
 * ===========================================================================
 *      Temporary structure for holding updated thread info
 * ===========================================================================
 */

/* used when constructing a new set of threads */
typedef struct {
    int         numThreads;     /* max #of threads */
    int         nextSlot;       /* where the next one goes */
    NuThread*   pThreads;       /* static-sized array */
} NuNewThreads;

/*
 * Allocate and initialize a NuNewThreads struct.
 */
static NuError Nu_NewThreads_New(NuArchive* pArchive,
    NuNewThreads** ppNewThreads, long numThreads)
{
    NuError err = kNuErrNone;

    *ppNewThreads = Nu_Malloc(pArchive, sizeof(**ppNewThreads));
    BailAlloc(*ppNewThreads);
    (*ppNewThreads)->numThreads = numThreads;
    (*ppNewThreads)->nextSlot = 0;
    (*ppNewThreads)->pThreads =
        Nu_Malloc(pArchive, numThreads * sizeof(NuThread));
    BailAlloc((*ppNewThreads)->pThreads);

bail:
    return err;
}

/*
 * Free a NuNewThreads struct.
 */
static void Nu_NewThreads_Free(NuArchive* pArchive, NuNewThreads* pNewThreads)
{
    if (pNewThreads != NULL) {
        Nu_Free(pArchive, pNewThreads->pThreads);
        Nu_Free(pArchive, pNewThreads);
    }
}

/*
 * Returns true if "pNewThreads" has room for another entry, false otherwise.
 */
static Boolean Nu_NewThreads_HasRoom(const NuNewThreads* pNewThreads)
{
    if (pNewThreads->nextSlot < pNewThreads->numThreads)
        return true;
    else
        return false;
}

/*
 * Get the next available slot.  The contents of the slot are first
 * initialized.
 *
 * The "next slot" marker is automatically advanced.
 */
static NuThread* Nu_NewThreads_GetNext(NuNewThreads* pNewThreads,
    NuArchive* pArchive)
{
    NuThread* pThread;

    pThread = &pNewThreads->pThreads[pNewThreads->nextSlot];
    memset(pThread, 0, sizeof(*pThread));

    pThread->fileOffset = -1;       /* mark as invalid */

    /* advance slot */
    pNewThreads->nextSlot++;
    Assert(pNewThreads->nextSlot <= pNewThreads->numThreads);

    return pThread;
}

/*
 * Return the #of threads we're meant to hold.
 */
static int Nu_NewThreads_GetNumThreads(const NuNewThreads* pNewThreads)
{
    Assert(pNewThreads != NULL);

    return pNewThreads->numThreads;
}

/*
 * Total up the compressed EOFs of all threads.
 */
static uint32_t Nu_NewThreads_TotalCompThreadEOF(NuNewThreads* pNewThreads)
{
    uint32_t compThreadEOF;
    int i;

    /* we should be all full up at this point; if not, we have a bug */
    Assert(pNewThreads != NULL);
    Assert(pNewThreads->numThreads == pNewThreads->nextSlot);

    compThreadEOF = 0;
    for (i = 0; i < pNewThreads->numThreads; i++)
        compThreadEOF += pNewThreads->pThreads[i].thCompThreadEOF;

    return compThreadEOF;
}


/*
 * "Donate" the thread collection to the caller.  This returns a pointer
 * to the thread array, and then nukes our copy of the pointer.  This
 * allows us to transfer ownership of the storage to the caller.
 */
static NuThread* Nu_NewThreads_DonateThreads(NuNewThreads* pNewThreads)
{
    NuThread* pThreads = pNewThreads->pThreads;

    pNewThreads->pThreads = NULL;
    return pThreads;
}


/*
 * ===========================================================================
 *      Archive construction - Record-level functions
 * ===========================================================================
 */

/*
 * Copy an entire record (threads and all) from the source archive to the
 * current offset in the temp file.
 *
 * Pass in the record from the *copy* set, not the original.
 */
static NuError Nu_CopyArchiveRecord(NuArchive* pArchive, NuRecord* pRecord)
{
    NuError err = kNuErrNone;
    long offsetAdjust;
    long outputOffset;
    int i;

    err = Nu_FTell(pArchive->tmpFp, &outputOffset);
    BailError(err);
    offsetAdjust = outputOffset - pRecord->fileOffset;

    DBUG(("--- Copying record '%s' (curOff=%ld adj=%ld)\n", pRecord->filename,
        outputOffset, offsetAdjust));

    /* seek to the start point in the source file, and copy the whole thing */
    err = Nu_FSeek(pArchive->archiveFp, pRecord->fileOffset, SEEK_SET);
    BailError(err);
    err = Nu_CopyFileSection(pArchive, pArchive->tmpFp, pArchive->archiveFp,
            pRecord->recHeaderLength + pRecord->totalCompLength);
    BailError(err);

    /* adjust the file offsets in the record header and in the threads */
    pRecord->fileOffset += offsetAdjust;

    for (i = 0; i < (int)pRecord->recTotalThreads; i++) {
        NuThread* pThread = Nu_GetThread(pRecord, i);

        pThread->fileOffset += offsetAdjust;
    }

    Assert(outputOffset + pRecord->recHeaderLength + pRecord->totalCompLength ==
        (uint32_t)ftell(pArchive->tmpFp));
    Assert(pRecord->fileOffset == outputOffset);

bail:
    return err;
}


/*
 * Count the number of threads that will eventually inhabit this record.
 *
 * Returns -1 on error.
 */
static NuError Nu_CountEventualThreads(const NuRecord* pRecord,
    long* pTotalThreads, long* pFilenameThreads)
{
    const NuThreadMod* pThreadMod;
    const NuThread* pThread;
    long idx, numThreads, numFilenameThreads;

    /*
     * Number of threads is equal to:
     *  the number of existing threads
     *  MINUS the number of "delete" threadMods (you can't delete the same
     *    thread more than once)
     *  PLUS the number of "add" threadMods
     */
    numThreads = pRecord->recTotalThreads;
    numFilenameThreads = 0;

    pThreadMod = pRecord->pThreadMods;
    while (pThreadMod != NULL) {
        switch (pThreadMod->entry.kind) {
        case kNuThreadModAdd:
            numThreads++;
            if (pThreadMod->entry.add.threadID == kNuThreadIDFilename)
                numFilenameThreads++;
            break;
        case kNuThreadModDelete:
            numThreads--;
            if (pThreadMod->entry.delete.threadID == kNuThreadIDFilename)
                numFilenameThreads--;
            break;
        case kNuThreadModUpdate:
            break;
        default:
            Assert(0);
            break;
        }

        pThreadMod = pThreadMod->pNext;
    }

    /*
     * If the record has more than one filename thread, we only keep
     * the first one, so remove it from our accounting here.  It should
     * not have been possible to add a new filename thread when an
     * existing one was present, so we don't check the threadMods.
     */
    for (idx = 0; idx < (long)pRecord->recTotalThreads; idx++) {
        pThread = Nu_GetThread(pRecord, idx);
        Assert(pThread != NULL);

        if (NuGetThreadID(pThread) == kNuThreadIDFilename)
            numFilenameThreads++;
    }
    Assert(numFilenameThreads >= 0);
    if (numFilenameThreads > 1) {
        DBUG(("--- ODD: found multiple filename threads (%ld)\n",
            numFilenameThreads));
        numThreads -= (numFilenameThreads -1);
    }

    /*
     * Records with no threads should've been screened out already.
     */
    if (numThreads <= 0)
        return kNuErrInternal;

    *pTotalThreads = numThreads;
    *pFilenameThreads = numFilenameThreads; /* [should cap this at 1?] */
    return kNuErrNone;
}


/*
 * Verify that all of the threads and threadMods in a record have
 * been touched.  This is done after the record has been written to
 * the destination archive, in order to ensure that we don't leave
 * anything behind.
 *
 * All items, including things like duplicate filename threads that
 * we ignore, are marked "used" during processing, so we don't need
 * to be terribly bright here.
 */
static Boolean Nu_VerifyAllTouched(NuArchive* pArchive, const NuRecord* pRecord)
{
    const NuThreadMod* pThreadMod;
    const NuThread* pThread;
    long idx;

    Assert(pArchive != NULL);
    Assert(pRecord != NULL);

    pThreadMod = pRecord->pThreadMods;
    while (pThreadMod != NULL) {
        Assert(pThreadMod->entry.generic.used == false ||
               pThreadMod->entry.generic.used == true);
        if (!pThreadMod->entry.generic.used)
            return false;
        pThreadMod = pThreadMod->pNext;
    }

    for (idx = 0; idx < (long)pRecord->recTotalThreads; idx++) {
        pThread = Nu_GetThread(pRecord, idx);
        Assert(pThread != NULL);

        Assert(pThread->used == false || pThread->used == true);
        if (!pThread->used)
            return false;
    }

    return true;
}


/*
 * Set the threadFilename field of a record to a new value.  This does
 * not affect the record header filename.
 *
 * This call should only be made after an "add" or "update" threadMod has
 * successfully completed.
 *
 * "newName" must be allocated storage, Mac OS Roman charset.
 */
static void Nu_SetNewThreadFilename(NuArchive* pArchive, NuRecord* pRecord,
    char* newNameMOR)
{
    Assert(pRecord != NULL);
    Assert(newNameMOR != NULL);

    Nu_Free(pArchive, pRecord->threadFilenameMOR);
    pRecord->threadFilenameMOR = newNameMOR;
    pRecord->filenameMOR = pRecord->threadFilenameMOR;
}

/*
 * If this is a disk image, we require that the uncompressed length
 * be equal to recExtraType * recStorageType (where recStorageType
 * is the block size, usually 512).  If they haven't set those to
 * appropriate values, we'll set them on their behalf, so long as
 * the uncompressed size is a multiple of 512.
 */
static NuError Nu_UpdateDiskImageFields(NuArchive* pArchive, NuRecord* pRecord,
    long sourceLen)
{
    NuError err = kNuErrNone;
    long actualLen;

    if (pRecord->recStorageType <= 13)
        pRecord->recStorageType = 512;
    actualLen = pRecord->recExtraType * pRecord->recStorageType;

    if (actualLen != sourceLen) {
        /* didn't match, see if we can fix it */
        DBUG(("--- fixing up disk image size\n"));
        if ((sourceLen & 0x1ff) == 0) {
            pRecord->recStorageType = 512;
            pRecord->recExtraType = sourceLen / 512;
        } else {
            /* oh dear */
            err = kNuErrBadData;
            Nu_ReportError(NU_BLOB, kNuErrNone,"disk image size of %ld invalid",
                sourceLen);
            /* fall through and out */
        }
    }

    return err;
}

/*
 * As part of thread construction or in-place updating, handle a single
 * "update" threadMod.  We have an existing thread, and are replacing
 * the contents of it with new data.
 *
 * "pThread" is a thread from the copy list or a "new" thread (a copy of
 * the thread from the "copy" list), and "pThreadMod" is a threadMod that
 * effects pThread.
 *
 * "fp" is a pointer into the archive at the offset where the data is
 * to be written.  On exit, "fp" will point past the end of the pre-sized
 * buffer.
 *
 * Possible side-effects on "pRecord": threadFilename may be updated.
 */
static NuError Nu_ConstructArchiveUpdate(NuArchive* pArchive, FILE* fp,
    NuRecord* pRecord, NuThread* pThread, const NuThreadMod* pThreadMod)
{
    NuError err;
    NuDataSource* pDataSource = NULL;
    uint32_t sourceLen;
    uint32_t threadBufSize;

    /*
     * We're going to copy the data out of the data source.  Because
     * "update" actions only operate on pre-sized chunks, and the data
     * is never compressed, this should be straightforward.  However,
     * we do need to make sure that the data will fit.
     *
     * I expect these to be small, and it's just a raw data copy, so no
     * progress updater is used.
     */
    Assert(Nu_IsPresizedThreadID(NuGetThreadID(pThread)));
    Assert(pThread->thCompThreadEOF >= pThread->thThreadEOF);
    threadBufSize = pThread->thCompThreadEOF;
    pDataSource = pThreadMod->entry.update.pDataSource;
    Assert(pDataSource != NULL);

    err = Nu_DataSourcePrepareInput(pArchive, pDataSource);
    if (err == kNuErrSkipped) {
        /* something failed (during file open?), just skip this one */
        DBUG(("--- skipping pre-sized thread update to %ld\n",
            pThread->threadIdx));
        err = kNuErrNone;
        goto skip_update;
    } else if (err != kNuErrNone)
        goto bail;

    /*
     * Check to see if the data will fit.  In some cases we can verify
     * the size during the UpdatePresizedThread call, but if it's being
     * added from a file we can't tell until now.
     *
     * We could be nice and give the user a chance to do something about
     * this, but frankly the application should have checked the file
     * size before handing it to us.
     */
    sourceLen = Nu_DataSourceGetDataLen(pDataSource);
    if (sourceLen > pThread->thCompThreadEOF) {
        err = kNuErrPreSizeOverflow;
        Nu_ReportError(NU_BLOB, err, "can't fit %u bytes into %u-byte buffer",
            sourceLen, pThread->thCompThreadEOF);
        goto bail;
    }

    /*
     * During an update operation, the user's specification of "otherLen"
     * doesn't really matter, because we're not going to change the size
     * of the region in the archive.  However, this size *is* used by
     * the code to figure out how big the buffer should be, and will
     * determine where the file pointer ends up when the call returns.
     * So, we jam in the "real" value.
     */
    Nu_DataSourceSetOtherLen(pDataSource, pThread->thCompThreadEOF);

    if (NuGetThreadID(pThread) == kNuThreadIDFilename) {
        /* special handling for filename updates */
        char* savedCopyMOR = NULL;
        err = Nu_CopyPresizedToArchive(pArchive, pDataSource,
                NuGetThreadID(pThread), fp, pThread, &savedCopyMOR);
        if (err != kNuErrNone) {
            Nu_ReportError(NU_BLOB, err, "thread update failed");
            goto bail;
        }
        Nu_SetNewThreadFilename(pArchive, pRecord, savedCopyMOR);

    } else {
        err = Nu_CopyPresizedToArchive(pArchive, pDataSource,
                NuGetThreadID(pThread), fp, pThread, NULL);
        if (err != kNuErrNone) {
            Nu_ReportError(NU_BLOB, err, "thread update failed");
            goto bail;
        }
    }
    Assert((uint32_t)ftell(fp) == pThread->fileOffset + threadBufSize);

skip_update:
    Nu_DataSourceUnPrepareInput(pArchive, pDataSource);

bail:
    return err;
}


/*
 * Handle all "add" threadMods in the current record.  This is invoked both
 * when creating a new record from the "new" list or constructing a
 * modified record from the "copy" list.
 *
 * Writes to either the archiveFp or tmpFp; pass in the correct one,
 * properly positioned.
 *
 * If something goes wrong with one of the "adds", this will return
 * immediately with kNuErrSkipped.  The caller is expected to abort the
 * entire record, so there's no point in continuing to process other
 * threads.
 *
 * Possible side-effects on "pRecord": disk image fields may be revised
 * (storage type, extra type), and threadFilename may be updated.
 */
static NuError Nu_HandleAddThreadMods(NuArchive* pArchive, NuRecord* pRecord,
    NuThreadID threadID, Boolean doKeepFirstOnly, NuNewThreads* pNewThreads,
    FILE* dstFp)
{
    NuError err = kNuErrNone;

    NuProgressData progressData;
    NuProgressData* pProgressData;
    NuThreadMod* pThreadMod;
    NuThread* pNewThread;
    UNICHAR* pathnameUNIStorage = NULL;
    Boolean foundOne = false;

    /*
     * Now find all "add" threadMods with matching threadIDs.  Allow
     * matching by wildcards, but don't re-use "used" entries.
     */
    pThreadMod = pRecord->pThreadMods;
    while (pThreadMod != NULL) {
        if (pThreadMod->entry.kind == kNuThreadModAdd &&
            !pThreadMod->entry.generic.used &&
            (pThreadMod->entry.add.threadID == threadID ||
             threadID == kNuThreadIDWildcard))
        {
            DBUG(("+++ found ADD for 0x%08lx\n", pThreadMod->entry.add.threadID));
            pThreadMod->entry.generic.used = true;

            /* if we're adding filename threads, stop after first one */
            /* [shouldn't be able to happen... we only allow one filename!] */
            if (doKeepFirstOnly && foundOne) {
                Assert(0);      /* can this happen?? */
                continue;
            }
            foundOne = true;

            if (!Nu_NewThreads_HasRoom(pNewThreads)) {
                Assert(0);
                err = kNuErrInternal;
                goto bail;
            }

            /* if this is a data thread, prepare the progress message */
            pProgressData = NULL;
            if (NuThreadIDGetClass(pThreadMod->entry.add.threadID) ==
                kNuThreadClassData)
            {
                /*
                 * We're going to show the name as it appears in the
                 * archive, rather than the name of the file we're
                 * reading the data out of.  We could do this differently
                 * for a "file" data source, but we might as well be
                 * consistent.
                 *
                 * [Actually, the above remark is bogus.  During a bulk add
                 * there's no other way to recover the original filename.
                 * Do something different here for data sinks with
                 * filenames attached. ++ATM 2003/02/17]
                 */
                pathnameUNIStorage = Nu_CopyMORToUNI(pRecord->filenameMOR);
                if (Nu_DataSourceGetType(pThreadMod->entry.add.pDataSource)
                    == kNuDataSourceFromFile)
                {
                    /* use on-disk filename */
                    err = Nu_ProgressDataInit_Compress(pArchive, &progressData,
                            pRecord, Nu_DataSourceFile_GetPathname(
                                pThreadMod->entry.add.pDataSource),
                            pathnameUNIStorage);
                } else {
                    /* use archive filename for both */
                    err = Nu_ProgressDataInit_Compress(pArchive, &progressData,
                            pRecord, pathnameUNIStorage, pathnameUNIStorage);
                }
                BailError(err);

                /* send initial progress so they see name if "open" fails */
                progressData.state = kNuProgressOpening;
                err = Nu_SendInitialProgress(pArchive, &progressData);
                BailError(err);

                pProgressData = &progressData;
            }

            /* get new thread storage, and init the thread's data offset */
            /* (the threadIdx is set by GetNext) */
            pNewThread = Nu_NewThreads_GetNext(pNewThreads, pArchive);
            pNewThread->threadIdx = pThreadMod->entry.add.threadIdx;
            err = Nu_FTell(dstFp, &pNewThread->fileOffset);
            BailError(err);

            /* this returns kNuErrSkipped if user elects to skip */
            err = Nu_DataSourcePrepareInput(pArchive,
                    pThreadMod->entry.add.pDataSource);
            BailError(err);

            /*
             * If they're adding a disk image thread, make sure the disk-
             * related fields in the record header are correct.
             */
            if (pThreadMod->entry.add.threadID == kNuThreadIDDiskImage) {
                const NuDataSource* pDataSource =
                    pThreadMod->entry.add.pDataSource;
                uint32_t uncompLen;

                if (Nu_DataSourceGetThreadFormat(pDataSource) ==
                                                    kNuThreadFormatUncompressed)
                {
                    uncompLen = Nu_DataSourceGetDataLen(pDataSource);
                } else {
                    uncompLen = Nu_DataSourceGetOtherLen(pDataSource);
                }

                err = Nu_UpdateDiskImageFields(pArchive, pRecord, uncompLen);
                BailError(err);
            }

            if (Nu_DataSourceGetType(pThreadMod->entry.add.pDataSource) ==
                kNuDataSourceFromFile)
            {
                DBUG(("+++ ADDING from '%s' for '%s' (idx=%ld id=0x%08lx)\n",
                    Nu_DataSourceFile_GetPathname(pThreadMod->entry.add.pDataSource),
                    pRecord->filename,
                    pThreadMod->entry.add.threadIdx,
                    pThreadMod->entry.add.threadID));
            } else {
                DBUG(("+++ ADDING from (type=%d) for '%s' (idx=%ld id=0x%08lx)\n",
                    Nu_DataSourceGetType(pThreadMod->entry.add.pDataSource),
                    pRecord->filename,
                    pThreadMod->entry.add.threadIdx,
                    pThreadMod->entry.add.threadID));
            }

            if (pThreadMod->entry.add.threadID == kNuThreadIDFilename) {
                /* filenames are special */
                char* savedCopyMOR = NULL;

                Assert(pThreadMod->entry.add.threadFormat ==
                    kNuThreadFormatUncompressed);
                err = Nu_CopyPresizedToArchive(pArchive,
                        pThreadMod->entry.add.pDataSource,
                        pThreadMod->entry.add.threadID,
                        dstFp, pNewThread, &savedCopyMOR);
                if (err != kNuErrNone) {
                    Nu_ReportError(NU_BLOB, err, "fn thread add failed");
                    goto bail;
                }
                /* NOTE: on failure, "dropRecFilename" is still set.  This
                   doesn't matter though, since we'll either copy the original
                   record, or abort the entire thing.  At any rate, we can't
                   just clear it, because we've already made space for the
                   record header, and didn't include the filename in it. */

                Nu_SetNewThreadFilename(pArchive, pRecord, savedCopyMOR);

            } else if (pThreadMod->entry.add.isPresized) {
                /* don't compress, just copy */
                Assert(pThreadMod->entry.add.threadFormat ==
                    kNuThreadFormatUncompressed);
                err = Nu_CopyPresizedToArchive(pArchive,
                        pThreadMod->entry.add.pDataSource,
                        pThreadMod->entry.add.threadID,
                        dstFp, pNewThread, NULL);
                /* fall through with err */

            } else {
                /* compress (possibly by just copying) the source to dstFp */
                err = Nu_CompressToArchive(pArchive,
                        pThreadMod->entry.add.pDataSource,
                        pThreadMod->entry.add.threadID,
                        Nu_DataSourceGetThreadFormat(
                            pThreadMod->entry.add.pDataSource),
                        pThreadMod->entry.add.threadFormat,
                        pProgressData, dstFp, pNewThread);
                /* fall through with err */
            }

            if (err != kNuErrNone) {
                Nu_ReportError(NU_BLOB, err, "thread add failed");
                goto bail;
            }
            Nu_DataSourceUnPrepareInput(pArchive,
                pThreadMod->entry.add.pDataSource);
        }

        pThreadMod = pThreadMod->pNext;
    }

bail:
    Nu_Free(pArchive, pathnameUNIStorage);
    return err;
}

/*
 * Run through the list of threads and threadMods, looking for threads
 * with an ID that matches "threadID".  When one is found, we take all
 * the appropriate steps to get the data into the archive.
 *
 * This takes into account the ThreadMods, including "delete" (ignore
 * existing thread), "update" (use data from threadMod instead of
 * existing thread), and "add" (use data from threadMod).
 *
 * Threads that are used or discarded will have a flag set so that
 * future examinations, notably those where "threadID" is a wildcard,
 * will ignore them.
 *
 * Always writes to the temp file.  The temp file must be positioned in
 * the proper location.
 *
 * "pRecord" must be from the "copy" data set.
 */
static NuError Nu_ConstructArchiveThreads(NuArchive* pArchive,
    NuRecord* pRecord, NuThreadID threadID, Boolean doKeepFirstOnly,
    NuNewThreads* pNewThreads)
{
    NuError err = kNuErrNone;
    NuThread* pThread;
    NuThreadMod* pThreadMod;
    Boolean foundOne = false;
    NuThread* pNewThread;
    int idx;

    /*
     * First, find any existing threads that match.  If they have a
     * "delete" threadMod, ignore them; if they have an "update" threadMod,
     * use that instead.
     */
    for (idx = 0; idx < (int)pRecord->recTotalThreads; idx++) {
        pThread = Nu_GetThread(pRecord, idx);
        Assert(pThread != NULL);

        DBUG(("+++ THREAD #%d (used=%d)\n", idx, pThread->used));
        if (threadID == kNuThreadIDWildcard ||
            threadID == NuGetThreadID(pThread))
        {
            /* match! */
            DBUG(("+++ MATCH THREAD #%d\n", idx));
            if (pThread->used)
                continue;
            pThread->used = true;   /* no matter what, we're done with this */

            pThreadMod = Nu_ThreadMod_FindByThreadIdx(pRecord,
                            pThread->threadIdx);

            if (pThreadMod != NULL) {
                /*
                 * The thread has a related ThreadMod.  Deal with it.
                 */

                pThreadMod->entry.generic.used = true;  /* for Assert, later */

                if (pThreadMod->entry.kind == kNuThreadModDelete) {
                    /* this is a delete, ignore this thread */
                    DBUG(("+++  deleted %ld!\n", pThread->threadIdx));
                    continue;
                } else if (pThreadMod->entry.kind == kNuThreadModUpdate) {
                    /* update pre-sized data in place */

                    DBUG(("+++  updating threadIdx=%ld\n",
                        pThread->threadIdx));

                    /* only one filename per customer */
                    /* [does this make sense here??] */
                    if (doKeepFirstOnly && foundOne)
                        continue;
                    foundOne = true;

                    /* add an entry in the new list of threads */
                    pNewThread = Nu_NewThreads_GetNext(pNewThreads, pArchive);
                    Nu_CopyThreadContents(pNewThread, pThread);

                    /* set the thread's file offset */
                    err = Nu_FTell(pArchive->tmpFp, &pNewThread->fileOffset);
                    BailError(err);

                    err = Nu_ConstructArchiveUpdate(pArchive, pArchive->tmpFp,
                            pRecord, pNewThread, pThreadMod);
                    BailError(err);
                } else {
                    /* unknown ThreadMod type - this shouldn't happen! */
                    Assert(0);
                    err = kNuErrInternal;
                    goto bail;
                }
            } else {
                /*
                 * Thread is unmodified.
                 */

                /* only one filename per customer */
                if (doKeepFirstOnly && foundOne)
                    continue;
                foundOne = true;

                /*
                 * Copy the original data to the new location.  Right now,
                 * pThread->fileOffset has the correct offset for the
                 * original file, and tmpFp is positioned at the correct
                 * output offset.  We want to seek the source file, replace
                 * pThread->fileOffset with the *new* offset, and then
                 * copy the data.
                 *
                 * This feels skankier than it really is because we're
                 * using the thread in the "copy" set for two purposes.
                 * It'd be cleaner to pass in the thread from the "orig"
                 * set, but there's really not much value in doing so.
                 *
                 * [should this have a progress meter associated?]
                 */
                DBUG(("+++  just copying threadIdx=%ld\n",
                    pThread->threadIdx));
                err = Nu_FSeek(pArchive->archiveFp, pThread->fileOffset,
                        SEEK_SET);
                BailError(err);
                err = Nu_FTell(pArchive->tmpFp, &pThread->fileOffset);
                BailError(err);
                err = Nu_CopyFileSection(pArchive, pArchive->tmpFp,
                        pArchive->archiveFp, pThread->thCompThreadEOF);
                BailError(err);

                /* copy an entry over into the replacement thread list */
                pNewThread = Nu_NewThreads_GetNext(pNewThreads, pArchive);
                Nu_CopyThreadContents(pNewThread, pThread);
            }
        }
    }

    /* no need to check for "add" mods; there can't be one for us */
    if (doKeepFirstOnly && foundOne)
        goto bail;

    /*
     * Now handle any "add" threadMods.
     */
    err = Nu_HandleAddThreadMods(pArchive, pRecord, threadID, doKeepFirstOnly,
            pNewThreads, pArchive->tmpFp);
    BailError(err);

bail:
    return err;
}

/*
 * Construct a record in the temp file, based on the contents of the
 * original.  Takes into account "dirty" headers and threadMod changes.
 *
 * Pass in the record from the *copy* set, not the original.  The temp
 * file should be positioned at the correct spot.
 *
 * If something goes wrong, and the user wants to abort the record but
 * not the entire operation, we rewind the temp file to the initial
 * position.  It's not possible to abandon part of a record; either you
 * get everything you asked for or nothing at all.  We then return
 * kNuErrSkipped, which should cause the caller to simply copy the
 * previous record.
 */
static NuError Nu_ConstructArchiveRecord(NuArchive* pArchive, NuRecord* pRecord)
{
    NuError err;
    NuNewThreads* pNewThreads = NULL;
    long threadDisp;
    long initialOffset, finalOffset;
    long numThreads, numFilenameThreads;
    int newHeaderSize;

    Assert(pArchive != NULL);
    Assert(pRecord != NULL);

    DBUG(("--- Reconstructing '%s'\n", pRecord->filename));

    err = Nu_FTell(pArchive->tmpFp, &initialOffset);
    BailError(err);
    Assert(initialOffset != 0);

    /*
     * Figure out how large the record header is.  This requires
     * measuring the static elements, the not-so-static elements like
     * the GS/OS option list and perhaps the filename, and getting an
     * accurate count of the number of threads.
     *
     * Since we're going to keep any option lists and extra junk stored in
     * the header originally, the size of the new base record header is
     * equal to the original recAttribCount.  The attribute count conveniently
     * does *not* include the filename, so if we've moved it out of the
     * record header and into a thread, it won't affect us here.
     */
    err = Nu_CountEventualThreads(pRecord, &numThreads, &numFilenameThreads);
    BailError(err);
    Assert(numThreads > 0);     /* threadless records should already be gone */
    if (numThreads <= 0) {
        err = kNuErrInternal;
        goto bail;
    }

    /*
     * Handle filename deletion.
     */
    if (!numFilenameThreads && pRecord->threadFilenameMOR != NULL) {
        /* looks like a previously existing filename thread got removed */
        DBUG(("--- Dropping thread filename '%s'\n",
            pRecord->threadFilenameMOR));
        if (pRecord->filenameMOR == pRecord->threadFilenameMOR)
            pRecord->filenameMOR = NULL;    /* don't point at freed memory! */
        Nu_Free(pArchive, pRecord->threadFilenameMOR);
        pRecord->threadFilenameMOR = NULL;

        /* I don't think this is possible, but check it anyway */
        if (pRecord->filenameMOR == NULL && pRecord->recFilenameMOR != NULL &&
            !pRecord->dropRecFilename)
        {
            DBUG(("--- HEY, how did this happen?\n"));
            pRecord->filenameMOR = pRecord->recFilenameMOR;
        }
    }
    if (pRecord->filenameMOR == NULL)
        pRecord->filenameMOR = kNuDefaultRecordName;

    /*
     * Make a hole, including the header filename if we're not dropping it.
     *
     * This ignores fake vs. non-fake threads, because once we're done
     * writing they're all "real".
     */
    newHeaderSize = pRecord->recAttribCount + numThreads * kNuThreadHeaderSize;
    if (!pRecord->dropRecFilename)
        newHeaderSize += pRecord->recFilenameLength;

    DBUG(("+++ new header size = %d\n", newHeaderSize));
    err = Nu_FSeek(pArchive->tmpFp, newHeaderSize, SEEK_CUR);
    BailError(err);

    /*
     * It is important to arrange the threads in a specific order.  For
     * example, we can have trouble doing a streaming archive read if the
     * filename isn't the first thread the collection.  It's prudent to
     * mimic GSHK's behavior, so we act to ensure that things appear in
     * the following order:
     *
     *  (1) filename thread
     *  (2) comment thread(s)
     *  (3) data thread with data fork
     *  (4) data thread with disk image
     *  (5) data thread with rsrc fork
     *  (6) everything else
     *
     * If we ended up with two filename threads (perhaps some other aberrant
     * application created the archive; we certainly wouldn't do that), we
     * keep the first one.  We're more lenient on propagating strange
     * multiple comment and data thread situations, even though the
     * thread updating mechanism in this library won't necessarily allow
     * such situations.
     */

    err = Nu_NewThreads_New(pArchive, &pNewThreads, numThreads);
    BailError(err);

    err = Nu_ConstructArchiveThreads(pArchive, pRecord, kNuThreadIDFilename,
            true, pNewThreads);
    BailError(err);
    err = Nu_ConstructArchiveThreads(pArchive, pRecord, kNuThreadIDComment,
            false, pNewThreads);
    BailError(err);
    err = Nu_ConstructArchiveThreads(pArchive, pRecord, kNuThreadIDDataFork,
            false, pNewThreads);
    BailError(err);
    err = Nu_ConstructArchiveThreads(pArchive, pRecord, kNuThreadIDDiskImage,
            false, pNewThreads);
    BailError(err);
    err = Nu_ConstructArchiveThreads(pArchive, pRecord, kNuThreadIDRsrcFork,
            false, pNewThreads);
    BailError(err);
    err = Nu_ConstructArchiveThreads(pArchive, pRecord, kNuThreadIDWildcard,
            false, pNewThreads);
    BailError(err);

    /*
     * Perform some sanity checks.
     */
    Assert(!Nu_NewThreads_HasRoom(pNewThreads));

    /* verify that all threads and threadMods have been touched */
    if (!Nu_VerifyAllTouched(pArchive, pRecord)) {
        err = kNuErrInternal;
        Assert(0);
        goto bail;
    }

    /* verify that file displacement is where it should be */
    threadDisp = (long)Nu_NewThreads_TotalCompThreadEOF(pNewThreads);
    err = Nu_FTell(pArchive->tmpFp, &finalOffset);
    BailError(err);
    Assert(finalOffset > initialOffset);
    if (finalOffset - (initialOffset + newHeaderSize) != threadDisp) {
        Nu_ReportError(NU_BLOB, kNuErrNone,
            "ERROR: didn't end up where expected (%ld %ld %ld)",
            initialOffset, finalOffset, threadDisp);
        err = kNuErrInternal;
        Assert(0);
        goto bail;
    }

    /*
     * Free existing Threads and ThreadMods, and move the list from
     * pNewThreads over.
     */
    Nu_Free(pArchive, pRecord->pThreads);
    Nu_FreeThreadMods(pArchive, pRecord);
    pRecord->pThreads = Nu_NewThreads_DonateThreads(pNewThreads);
    pRecord->recTotalThreads = Nu_NewThreads_GetNumThreads(pNewThreads);

    /*
     * Now, seek back and write the record header.
     */
    err = Nu_FSeek(pArchive->tmpFp, initialOffset, SEEK_SET);
    BailError(err);
    err = Nu_WriteRecordHeader(pArchive, pRecord, pArchive->tmpFp);
    BailError(err);

    Assert(newHeaderSize == (int) pRecord->recHeaderLength);

    /*
     * Seek forward once again, so we are positioned at the correct
     * place to write the next record.
     */
    err = Nu_FSeek(pArchive->tmpFp, finalOffset, SEEK_SET);
    BailError(err);

    /* update the record's fileOffset to reflect its new position */
    DBUG(("+++ record shifted by %ld bytes\n",
        initialOffset - pRecord->fileOffset));
    pRecord->fileOffset = initialOffset;

bail:
    if (err == kNuErrSkipped) {
        /*
         * Something went wrong and they want to skip this record but
         * keep going otherwise.  We need to back up in the file so the
         * original copy of the record can go here.
         */
        err = Nu_FSeek(pArchive->tmpFp, initialOffset, SEEK_SET);
        if (err == kNuErrNone)
            err = kNuErrSkipped;    /* tell the caller we skipped it */
    }

    Nu_NewThreads_Free(pArchive, pNewThreads);
    return err;
}


/*
 * Construct a new record and add it to the original or temp file.  The
 * new record has no threads but some number of threadMods.  (This
 * function is a cousin to Nu_ConstructArchiveRecord.)  "pRecord" must
 * come from the "new" record set.
 *
 * The original/temp file should be positioned at the correct spot.
 *
 * If something goes wrong, and the user wants to abort the record but
 * not the entire operation, we rewind the temp file to the initial
 * position and return kNuErrSkipped.
 */
static NuError Nu_ConstructNewRecord(NuArchive* pArchive, NuRecord* pRecord,
    FILE* fp)
{
    NuError err;
    NuNewThreads* pNewThreads = NULL;
    NuThreadMod* pThreadMod;
    long threadDisp;
    long initialOffset, finalOffset;
    long numThreadMods, numFilenameThreads;
    int newHeaderSize;

    Assert(pArchive != NULL);
    Assert(pRecord != NULL);

    DBUG(("--- Constructing '%s'\n", pRecord->filename));

    err = Nu_FTell(fp, &initialOffset);
    BailError(err);
    Assert(initialOffset != 0);

    /*
     * Quick sanity check: verify that the record has no threads of its
     * own, and all threadMods are "add" threadMods.  While we're at it,
     * make ourselves useful by counting up the number of eventual
     * threads, and verify that there is exactly one filename thread.
     */
    Assert(pRecord->pThreads == NULL);

    numThreadMods = 0;
    numFilenameThreads = 0;
    pThreadMod = pRecord->pThreadMods;
    while (pThreadMod) {
        if (pThreadMod->entry.kind != kNuThreadModAdd) {
            Nu_ReportError(NU_BLOB, kNuErrNone, "unexpected non-add threadMod");
            err = kNuErrInternal;
            Assert(0);
            goto bail;
        }
        numThreadMods++;
        if (pThreadMod->entry.add.threadID == kNuThreadIDFilename)
            numFilenameThreads++;

        pThreadMod = pThreadMod->pNext;
    }
    Assert(numFilenameThreads <= 1);

    /*
     * If there's no filename thread, make one.  We do this for brand-new
     * records when the application doesn't explicitly add a thread.
     */
    if (!numFilenameThreads) {
        NuDataSource* pTmpDataSource = NULL;
        NuThreadMod* pNewThreadMod = NULL;
        int len, maxLen;

        /*
         * Generally speaking, the "add file" call should set the
         * filename.  If somehow it didn't, assign a default.
         */
        if (pRecord->filenameMOR == NULL) {
            pRecord->newFilenameMOR = strdup(kNuDefaultRecordName);
            pRecord->filenameMOR = pRecord->newFilenameMOR;
        }

        DBUG(("--- No filename thread found, adding one ('%s')\n",
            pRecord->filenameMOR));

        /*
         * Create a trivial data source for the filename.  The size of
         * the filename buffer is the larger of the filename length and
         * the default filename buffer size.  This mimics GSHK's behavior.
         * (If we're really serious about renaming it, maybe we should
         * leave some extra space on the end...?)
         */
        len = strlen(pRecord->filenameMOR);
        maxLen = len > kNuDefaultFilenameThreadSize ?
                                            len : kNuDefaultFilenameThreadSize;
        err = Nu_DataSourceBuffer_New(kNuThreadFormatUncompressed,
                maxLen, (const uint8_t*)pRecord->filenameMOR, 0,
                strlen(pRecord->filenameMOR), NULL, &pTmpDataSource);
        BailError(err);

        /* put in a new "add" threadMod (which copies the data source) */
        err = Nu_ThreadModAdd_New(pArchive, kNuThreadIDFilename,
                kNuThreadFormatUncompressed, pTmpDataSource, &pNewThreadMod);
        Nu_DataSourceFree(pTmpDataSource);
        BailError(err);

        /* add it to the list */
        Nu_RecordAddThreadMod(pRecord, pNewThreadMod);
        pNewThreadMod = NULL;

        numFilenameThreads++;
        numThreadMods++;
    }

    /*
     * Figure out how large the record header is.  We don't generate
     * GS/OS option lists or "extra" data here, and we always put the
     * filename in a thread, so the size is constant.  (If somebody
     * does a GS/OS or Mac port and wants to add option lists, it should
     * not be hard to adjust the size accordingly.)
     *
     * This initializes the record's attribCount.  We use the "base size"
     * and add two for the (unused) filename length.
     */
    pRecord->recAttribCount = kNuRecordHeaderBaseSize +2;
    newHeaderSize = pRecord->recAttribCount + numThreadMods * kNuThreadHeaderSize;

    DBUG(("+++ new header size = %d\n", newHeaderSize));

    /* leave a hole */
    err = Nu_FSeek(fp, newHeaderSize, SEEK_CUR);
    BailError(err);

    /*
     * It is important to arrange the threads in a specific order.  See
     * the comments in Nu_ConstructArchiveRecord for the rationale.
     */
    err = Nu_NewThreads_New(pArchive, &pNewThreads, numThreadMods);
    BailError(err);

    err = Nu_HandleAddThreadMods(pArchive, pRecord, kNuThreadIDFilename,
            true, pNewThreads, fp);
    BailError(err);
    err = Nu_HandleAddThreadMods(pArchive, pRecord, kNuThreadIDComment,
            false, pNewThreads, fp);
    BailError(err);
    err = Nu_HandleAddThreadMods(pArchive, pRecord, kNuThreadIDDataFork,
            false, pNewThreads, fp);
    BailError(err);
    err = Nu_HandleAddThreadMods(pArchive, pRecord, kNuThreadIDDiskImage,
            false, pNewThreads, fp);
    BailError(err);
    err = Nu_HandleAddThreadMods(pArchive, pRecord, kNuThreadIDRsrcFork,
            false, pNewThreads, fp);
    BailError(err);
    err = Nu_HandleAddThreadMods(pArchive, pRecord, kNuThreadIDWildcard,
            false, pNewThreads, fp);
    BailError(err);

    /*
     * Perform some sanity checks.
     */
    Assert(!Nu_NewThreads_HasRoom(pNewThreads));

    /* verify that all threads and threadMods have been touched */
    if (!Nu_VerifyAllTouched(pArchive, pRecord)) {
        err = kNuErrInternal;
        Assert(0);
        goto bail;
    }

    /* verify that file displacement is where it should be */
    threadDisp = Nu_NewThreads_TotalCompThreadEOF(pNewThreads);
    err = Nu_FTell(fp, &finalOffset);
    BailError(err);
    Assert(finalOffset > initialOffset);
    if (finalOffset - (initialOffset + newHeaderSize) != threadDisp) {
        Nu_ReportError(NU_BLOB, kNuErrNone,
            "ERROR: didn't end up where expected (%ld %ld %ld)",
            initialOffset, finalOffset, threadDisp);
        err = kNuErrInternal;
        Assert(0);
        goto bail;
    }

    /*
     * Install pNewThreads as the thread list.
     */
    Assert(pRecord->pThreads == NULL && pRecord->recTotalThreads == 0);
    pRecord->pThreads = Nu_NewThreads_DonateThreads(pNewThreads);
    pRecord->recTotalThreads = Nu_NewThreads_GetNumThreads(pNewThreads);

    /*
     * Fill in misc record header fields.
     *
     * We could set recArchiveWhen here, if we wanted to override what
     * the application set, but I don't think there's any value in that.
     */
    pRecord->fileOffset = initialOffset;

    /*
     * Now, seek back and write the record header.
     */
    err = Nu_FSeek(fp, initialOffset, SEEK_SET);
    BailError(err);
    err = Nu_WriteRecordHeader(pArchive, pRecord, fp);
    BailError(err);

    /*
     * Seek forward once again, so we are positioned at the correct
     * place to write the next record.
     */
    err = Nu_FSeek(fp, finalOffset, SEEK_SET);
    BailError(err);

    /*
     * Trash the threadMods.
     */
    Nu_FreeThreadMods(pArchive, pRecord);

bail:
    if (err == kNuErrSkipped) {
        /*
         * Something went wrong and they want to skip this record but
         * keep going otherwise.  We need to back up in the file so the
         * next record can go here.
         */
        err = Nu_FSeek(fp, initialOffset, SEEK_SET);
        if (err == kNuErrNone)
            err = kNuErrSkipped;    /* tell the caller we skipped it */
    }

    Nu_NewThreads_Free(pArchive, pNewThreads);
    return err;
}


/*
 * Update a given record in the original archive file.
 *
 * "pRecord" is the record from the "copy" set.  It can have the
 * "dirtyHeader" flag set, and may have "update" threadMods, but
 * that's all.
 *
 * The position of pArchive->archiveFp on entry and on exit is not
 * defined.
 */
static NuError Nu_UpdateRecordInOriginal(NuArchive* pArchive, NuRecord* pRecord)
{
    NuError err = kNuErrNone;
    NuThread* pThread;
    const NuThreadMod* pThreadMod;

    /*
     * Loop through all threadMods.
     */
    pThreadMod = pRecord->pThreadMods;
    while (pThreadMod != NULL) {
        Assert(pThreadMod->entry.kind == kNuThreadModUpdate);

        /* find the thread associated with this threadMod */
        err = Nu_FindThreadByIdx(pRecord, pThreadMod->entry.update.threadIdx,
                &pThread);
        BailError(err);     /* should never happen */

        /* seek to the appropriate spot */
        err = Nu_FSeek(pArchive->archiveFp, pThread->fileOffset, SEEK_SET);
        BailError(err);

        /* do the update; this updates "pThread" with the new info */
        err = Nu_ConstructArchiveUpdate(pArchive, pArchive->archiveFp,
                pRecord, pThread, pThreadMod);
        BailError(err);

        pThreadMod = pThreadMod->pNext;
    }


    /*
     * We have to write a new record header without disturbing
     * anything around it.  Nothing we've done should've changed
     * the size of the record header, so just go ahead and write it.
     *
     * We have to do this regardless of "dirtyHeader", because we just
     * tweaked some of our threads around, and we need to rewrite the
     * thread headers (which updates the record header CRC, and so on).
     */
    err = Nu_FSeek(pArchive->archiveFp, pRecord->fileOffset, SEEK_SET);
    BailError(err);
    err = Nu_WriteRecordHeader(pArchive, pRecord, pArchive->archiveFp);
    BailError(err);

    /*
     * Let's be paranoid and verify that the write didn't overflow
     * into the thread header.  We compare our current offset against
     * the offset of the first thread.  (If we're in a weird record
     * with no threads, we could compare against the offset of the
     * next record, but I don't want to deal with a case that should
     * never happen anyway.)
     */
    DBUG(("--- record header wrote %ld bytes\n",
        pArchive->currentOffset - pRecord->fileOffset));
    pThread = pRecord->pThreads;
    if (pThread != NULL && pArchive->currentOffset != pThread->fileOffset) {
        /* guess what, we just trashed the archive */
        err = kNuErrDamaged;
        Nu_ReportError(NU_BLOB, err,
            "Bad record header write (off by %ld), archive damaged",
            pArchive->currentOffset - pThread->fileOffset);
        goto bail;
    }
    DBUG(("--- record header written safely\n"));


    /*
     * It's customary to throw out the thread mods when you're done.  (I'm
     * not really sure why I'm doing this now, but here we are.)
     */
    Nu_FreeThreadMods(pArchive, pRecord);

bail:
    return err;
}


/*
 * ===========================================================================
 *      Archive construction - main functions
 * ===========================================================================
 */

/*
 * Fill in the temp file with the contents of the original archive.  The
 * file offsets and any other generated data in the "copy" set will be
 * updated as appropriate, so that the "copy" set can eventually replace
 * the "orig" set.
 *
 * On exit, pArchive->tmpFp will point at the archive EOF.
 */
static NuError Nu_CreateTempFromOriginal(NuArchive* pArchive)
{
    NuError err = kNuErrNone;
    NuRecord* pRecord;

    Assert(pArchive->tmpFp != 0);
    Assert(ftell(pArchive->tmpFp) == 0);    /* should be empty as well */

    /*
     * Leave space for the master header and (if we're preserving it) any
     * header gunk.
     */
    Assert(!pArchive->valDiscardWrapper || pArchive->headerOffset == 0);
    err = Nu_FSeek(pArchive->tmpFp,
            pArchive->headerOffset + kNuMasterHeaderSize, SEEK_SET);
    BailError(err);

    if (Nu_RecordSet_GetLoaded(&pArchive->copyRecordSet)) {
        /*
         * Run through the "copy" records.  If the original record header is
         * umodified, just copy it; otherwise write a new one with a new CRC.
         */
        if (Nu_RecordSet_IsEmpty(&pArchive->copyRecordSet)) {
            /* new archive or all records deleted */
            DBUG(("--- No records in 'copy' set\n"));
            goto bail;
        }
        pRecord = Nu_RecordSet_GetListHead(&pArchive->copyRecordSet);
    } else {
        /*
         * There's no "copy" set defined.  If we have an "orig" set, we
         * must be doing nothing but add files to an existing archive
         * without the "modify orig" flag set.
         */
        if (Nu_RecordSet_IsEmpty(&pArchive->origRecordSet)) {
            DBUG(("--- No records in 'copy' or 'orig' set\n"));
            goto bail;
        }
        pRecord = Nu_RecordSet_GetListHead(&pArchive->origRecordSet);
    }

    /*
     * Reconstruct or copy the records.  It's probably not necessary
     * to reconstruct the entire record if we're just updating the
     * record header, but since all we do is copy the data anyway,
     * it's not much slower.
     */
    while (pRecord != NULL) {
        if (!pRecord->dirtyHeader && pRecord->pThreadMods == NULL) {
            err = Nu_CopyArchiveRecord(pArchive, pRecord);
            BailError(err);
        } else {
            err = Nu_ConstructArchiveRecord(pArchive, pRecord);
            if (err == kNuErrSkipped) {
                /*
                 * We're going to retain the original.  This requires us
                 * to copy the original record from the "orig" record set
                 * and replace what we had in the "copy" set, so that at
                 * the end of the day the "copy" set accurately reflects
                 * what's in the archive.
                 */
                DBUG(("--- Skipping, copying %ld instead\n",
                    pRecord->recordIdx));
                err = Nu_RecordSet_ReplaceRecord(pArchive,
                        &pArchive->copyRecordSet, pRecord,
                        &pArchive->origRecordSet, &pRecord);
                BailError(err);
                err = Nu_CopyArchiveRecord(pArchive, pRecord);
                BailError(err);
            }
            BailError(err);
        }

        pRecord = pRecord->pNext;
    }

bail:
    return err;
}


/*
 * Perform updates to certain items in the original archive.  None of
 * the operations changes the position of items within.
 *
 * On exit, pArchive->archiveFp will point at the archive EOF.
 */
static NuError Nu_UpdateInOriginal(NuArchive* pArchive)
{
    NuError err = kNuErrNone;
    NuRecord* pRecord;

    if (!Nu_RecordSet_GetLoaded(&pArchive->copyRecordSet)) {
        /*
         * There's nothing for us to do; we probably just have a
         * bunch of new stuff being added.
         */
        DBUG(("--- UpdateInOriginal: nothing to do\n"));
        goto done;
    }

    /*
     * Run through and process all the updates.
     */
    pRecord = Nu_RecordSet_GetListHead(&pArchive->copyRecordSet);
    while (pRecord != NULL) {
        if (pRecord->dirtyHeader || pRecord->pThreadMods != NULL) {
            err = Nu_UpdateRecordInOriginal(pArchive, pRecord);
            BailError(err);
        }

        pRecord = pRecord->pNext;
    }

done:
    /* seek to the end of the archive */
    err = Nu_FSeek(pArchive->archiveFp,
            pArchive->headerOffset + pArchive->masterHeader.mhMasterEOF,
            SEEK_SET);
    BailError(err);

bail:
    return err;
}


/*
 * Create new records for all items in the "new" list, writing them to
 * "fp" at the current offset.
 *
 * On completion, "fp" will point at the end of the archive.
 */
static NuError Nu_CreateNewRecords(NuArchive* pArchive, FILE* fp)
{
    NuError err = kNuErrNone;
    NuRecord* pRecord;

    pRecord = Nu_RecordSet_GetListHead(&pArchive->newRecordSet);
    while (pRecord != NULL) {
        err = Nu_ConstructNewRecord(pArchive, pRecord, fp);
        if (err == kNuErrSkipped) {
            /*
             * We decided to skip this record, so delete it from "new".
             *
             * (I think this is the only time we delete something from the
             * "new" set...)
             */
            NuRecord* pNextRecord = pRecord->pNext;

            DBUG(("--- Skipping, deleting new %ld\n", pRecord->recordIdx));
            err = Nu_RecordSet_DeleteRecord(pArchive, &pArchive->newRecordSet,
                    pRecord);
            Assert(err == kNuErrNone);
            BailError(err);
            pRecord = pNextRecord;
        } else {
            BailError(err);
            pRecord = pRecord->pNext;
        }
    }

bail:
    return err;
}


/*
 * ===========================================================================
 *      Archive update helpers
 * ===========================================================================
 */

/*
 * Determine if any "heavy updates" have been made.  A "heavy" update is
 * one that requires us to create and rename a temp file.
 *
 * If the "copy" record set hasn't been loaded, we're done.  If it has
 * been loaded, we scan through the list for thread mods other than updates
 * to pre-sized fields.  We also have to check to see if any records were
 * deleted.
 *
 * At present, a "dirtyHeader" flag is not of itself cause to rebuild
 * the archive, so we don't test for it here.
 */
static Boolean Nu_NoHeavyUpdates(NuArchive* pArchive)
{
    const NuRecord* pRecord;
    long count;

    /* if not loaded, then *no* changes were made to original records */
    if (!Nu_RecordSet_GetLoaded(&pArchive->copyRecordSet))
        return true;

    /*
     * You can't add to "copy" set, so any deletions are visible by the
     * reduced record count.  The function that deletes records from
     * which all threads have been removed should be called before we
     * get here.
     */
    if (Nu_RecordSet_GetNumRecords(&pArchive->copyRecordSet) !=
        Nu_RecordSet_GetNumRecords(&pArchive->origRecordSet))
    {
        return false;
    }

    /*
     * Run through the set of records, looking for a threadMod with a
     * change type we can't handle in place.
     */
    count = Nu_RecordSet_GetNumRecords(&pArchive->copyRecordSet);
    pRecord = Nu_RecordSet_GetListHead(&pArchive->copyRecordSet);
    while (count--) {
        const NuThreadMod* pThreadMod;

        Assert(pRecord != NULL);

        pThreadMod = pRecord->pThreadMods;
        while (pThreadMod != NULL) {
            /* the only acceptable kind is "update" */
            if (pThreadMod->entry.kind != kNuThreadModUpdate)
                return false;

            pThreadMod = pThreadMod->pNext;
        }

        pRecord = pRecord->pNext;
    }

    return true;
}


/*
 * Purge any records that don't have any threads.  This has to take into
 * account pending modifications, so that we dispose of any records that
 * have had all of their threads deleted.
 *
 * Simplest approach is to count up the #of "delete" mods and subtract
 * it from the number of threads, skipping on if the record has any
 * "add" thread mods.
 */
static NuError Nu_PurgeEmptyRecords(NuArchive* pArchive,
    NuRecordSet* pRecordSet)
{
    NuError err = kNuErrNone;
    NuRecord* pRecord;
    NuRecord** ppRecord;

    Assert(pArchive != NULL);
    Assert(pRecordSet != NULL);

    if (Nu_RecordSet_IsEmpty(pRecordSet))
        return kNuErrNone;

    ppRecord = Nu_RecordSet_GetListHeadPtr(pRecordSet);
    Assert(ppRecord != NULL);
    Assert(*ppRecord != NULL);

    /* maintain a pointer to the pointer, so we can delete easily */
    while (*ppRecord != NULL) {
        pRecord = *ppRecord;

        if (Nu_RecordIsEmpty(pArchive, pRecord)) {
            DBUG(("--- Purging empty record %06ld '%s' (0x%08lx-->0x%08lx)\n",
                pRecord->recordIdx, pRecord->filename,
                (uint32_t)ppRecord, (uint32_t)pRecord));
            err = Nu_RecordSet_DeleteRecordPtr(pArchive, pRecordSet, ppRecord);
            BailError(err);
            /* pRecord is now invalid, and *ppRecord has been updated */
        } else {
            ppRecord = &pRecord->pNext;
        }
    }

bail:
    return err;
}


/*
 * Update the "new" master header block with the contents of the modified
 * archive, and write it to the file.
 *
 * Pass in a correctly positioned "fp" and the total length of the archive
 * file.
 */
static NuError Nu_UpdateMasterHeader(NuArchive* pArchive, FILE* fp,
    long archiveEOF)
{
    NuError err;
    long numRecords;

    Nu_MasterHeaderCopy(pArchive, &pArchive->newMasterHeader,
        &pArchive->masterHeader);

    numRecords = 0;
    if (Nu_RecordSet_GetLoaded(&pArchive->copyRecordSet))
        numRecords += Nu_RecordSet_GetNumRecords(&pArchive->copyRecordSet);
    else
        numRecords += Nu_RecordSet_GetNumRecords(&pArchive->origRecordSet);
    if (Nu_RecordSet_GetLoaded(&pArchive->newRecordSet))
        numRecords += Nu_RecordSet_GetNumRecords(&pArchive->newRecordSet);
    #if 0   /* we allow delete-all now */
    if (numRecords == 0) {
        /* don't allow empty archives */
        DBUG(("--- UpdateMasterHeader didn't find any records\n"));
        err = kNuErrNoRecords;
        goto bail;
    }
    #endif

    pArchive->newMasterHeader.mhTotalRecords = numRecords;
    pArchive->newMasterHeader.mhMasterEOF = archiveEOF;
    pArchive->newMasterHeader.mhMasterVersion = kNuOurMHVersion;
    Nu_SetCurrentDateTime(&pArchive->newMasterHeader.mhArchiveModWhen);

    err = Nu_WriteMasterHeader(pArchive, fp, &pArchive->newMasterHeader);
    BailError(err);

bail:
    return err;
}


/*
 * Reset the temp file to a known (empty) state.
 */
static NuError Nu_ResetTempFile(NuArchive* pArchive)
{
    NuError err = kNuErrNone;

    /* read-only archives don't have a temp file */
    if (Nu_IsReadOnly(pArchive))
        return kNuErrNone;  /* or kNuErrArchiveRO? */

    Assert(pArchive != NULL);
    Assert(pArchive->tmpPathnameUNI != NULL);

#if 0   /* keep the temp file around for examination */
if (pArchive->tmpFp != NULL) {
    DBUG(("--- NOT Resetting temp file\n"));
    fflush(pArchive->tmpFp);
    goto bail;
}
#endif

    DBUG(("--- Resetting temp file\n"));

    /* if we renamed the temp over the original, we need to open a new temp */
    if (pArchive->tmpFp == NULL) {
        // as in Nu_OpenTempFile, skip the wchar conversion for the temp
        // file name, which we lazily assume to be ASCII
        pArchive->tmpFp = fopen(pArchive->tmpPathnameUNI,
                kNuFileOpenReadWriteCreat);
        if (pArchive->tmpFp == NULL) {
            err = errno ? errno : kNuErrFileOpen;
            Nu_ReportError(NU_BLOB, errno, "Unable to open temp file '%s'",
                pArchive->tmpPathnameUNI);
            goto bail;
        }
    } else {
        /*
         * Truncate the temp file.
         */
        err = Nu_FSeek(pArchive->tmpFp, 0, SEEK_SET);
        BailError(err);
        err = Nu_TruncateOpenFile(pArchive->tmpFp, 0);
        if (err == kNuErrInternal) {
            /* do it the hard way if we don't have ftruncate or equivalent */
            err = kNuErrNone;
            fclose(pArchive->tmpFp);
            pArchive->tmpFp = fopen(pArchive->tmpPathnameUNI,
                    kNuFileOpenWriteTrunc);
            if (pArchive->tmpFp == NULL) {
                err = errno ? errno : kNuErrFileOpen;
                Nu_ReportError(NU_BLOB, err, "failed truncating tmp file");
                goto bail;
            }
            fclose(pArchive->tmpFp);
            pArchive->tmpFp = fopen(pArchive->tmpPathnameUNI,
                    kNuFileOpenReadWriteCreat);
            if (pArchive->tmpFp == NULL) {
                err = errno ? errno : kNuErrFileOpen;
                Nu_ReportError(NU_BLOB, err, "Unable to open temp file '%s'",
                    pArchive->tmpPathnameUNI);
                goto bail;
            }
        }
    }

bail:
    return err;
}

/*
 * Ensure that all of the threads and threadMods in a record are in
 * a pristine state, i.e. "threads" aren't marked used and "threadMods"
 * don't even exist.  This is done as we are cleaning up the record sets
 * after a successful (or aborted) update.
 */
static NuError Nu_RecordResetUsedFlags(NuArchive* pArchive, NuRecord* pRecord)
{
    NuThread* pThread;
    long idx;

    Assert(pArchive != NULL);
    Assert(pRecord != NULL);

    /* these should already be clear */
    if (pRecord->pThreadMods) {
        Assert(0);
        return kNuErrInternal;
    }

    /* these might still be set */
    for (idx = 0; idx < (long)pRecord->recTotalThreads; idx++) {
        pThread = Nu_GetThread(pRecord, idx);
        Assert(pThread != NULL);

        pThread->used = false;
    }

    /* and this */
    pRecord->dirtyHeader = false;

    return kNuErrNone;
}

/*
 * Invoke Nu_RecordResetUsedFlags on all records in a record set.
 */
static NuError Nu_ResetUsedFlags(NuArchive* pArchive, NuRecordSet* pRecordSet)
{
    NuError err = kNuErrNone;
    NuRecord* pRecord;

    pRecord = Nu_RecordSet_GetListHead(pRecordSet);
    while (pRecord != NULL) {
        err = Nu_RecordResetUsedFlags(pArchive, pRecord);
        if (err != kNuErrNone) {
            Assert(0);
            break;
        }

        pRecord = pRecord->pNext;
    }

    return err;
}


/*
 * If nothing in the "copy" set has actually been disturbed, throw it out.
 */
static void Nu_ResetCopySetIfUntouched(NuArchive* pArchive)
{
    const NuRecord* pRecord;

    /* have any records been deleted? */
    if (Nu_RecordSet_GetNumRecords(&pArchive->copyRecordSet) !=
        pArchive->masterHeader.mhTotalRecords)
    {
        return;
    }

    /* do we have any thread mods or dirty record headers? */
    pRecord = Nu_RecordSet_GetListHead(&pArchive->copyRecordSet);
    while (pRecord != NULL) {
        if (pRecord->pThreadMods != NULL || pRecord->dirtyHeader)
            return;

        pRecord = pRecord->pNext;
    }

    /* looks like nothing has been touched */
    DBUG(("--- copy set untouched, trashing it\n"));
    (void) Nu_RecordSet_FreeAllRecords(pArchive, &pArchive->copyRecordSet);
}


/* 
 * GSHK always adds a comment to the first new record added to an archive.
 * Imitate this behavior.
 */
static NuError Nu_AddCommentToFirstNewRecord(NuArchive* pArchive)
{
    NuError err = kNuErrNone;
    NuRecord* pRecord;
    NuThreadMod* pThreadMod = NULL;
    NuThreadMod* pExistingThreadMod = NULL;
    NuDataSource* pDataSource = NULL;

    /* if there aren't any records there, skip this */
    if (Nu_RecordSet_IsEmpty(&pArchive->newRecordSet))
        goto bail;

    pRecord = Nu_RecordSet_GetListHead(&pArchive->newRecordSet);

    /*
     * See if this record already has a comment.  If so, don't add
     * another one.
     */
    err = Nu_ThreadModAdd_FindByThreadID(pRecord, kNuThreadIDComment,
            &pExistingThreadMod);
    if (err == kNuErrNone) {
        DBUG(("+++ record already has a comment, not adding another\n"));
        goto bail;          /* already exists */
    }
    err = kNuErrNone;

    /* create a new data source with nothing in it */
    err = Nu_DataSourceBuffer_New(kNuThreadFormatUncompressed,
            kNuDefaultCommentSize, NULL, 0, 0, NULL, &pDataSource);
    BailError(err);
    Assert(pDataSource != NULL);

    /* create a new ThreadMod */
    err = Nu_ThreadModAdd_New(pArchive, kNuThreadIDComment,
            kNuThreadFormatUncompressed, pDataSource, &pThreadMod);
    BailError(err);
    Assert(pThreadMod != NULL);
    /*pDataSource = NULL;*/  /* ThreadModAdd_New makes a copy */

    /* add the thread mod to the record */
    Nu_RecordAddThreadMod(pRecord, pThreadMod);
    pThreadMod = NULL;   /* don't free on exit */

bail:
    Nu_ThreadModFree(pArchive, pThreadMod);
    Nu_DataSourceFree(pDataSource);
    return err;
}


/*
 * ===========================================================================
 *      Main entry points
 * ===========================================================================
 */

/*
 * Force all deferred changes to occur.
 *
 * If the flush fails, the archive state may be aborted or even placed
 * into read-only mode to prevent problems from compounding.
 *
 * If the things this function is doing aren't making any sense at all,
 * read "NOTES.txt" for an introduction.
 */
NuError Nu_Flush(NuArchive* pArchive, uint32_t* pStatusFlags)
{
    NuError err = kNuErrNone;
    Boolean canAbort = true;
    Boolean writeToTemp = true;
    Boolean deleteAll = false;
    long initialEOF, finalOffset;

    DBUG(("--- FLUSH\n"));

    if (pStatusFlags == NULL)
        return kNuErrInvalidArg;
    /* these do get set on error, so clear them no matter what */
    *pStatusFlags = 0;

    if (Nu_IsReadOnly(pArchive))
        return kNuErrArchiveRO;

    err = Nu_GetFileLength(pArchive, pArchive->archiveFp, &initialEOF);
    BailError(err);

    /*
     * Step 1: figure out if we have anything to do.  If the "copy" and "new"
     * lists are empty, then there's nothing for us to do.
     *
     * As a special case, we test for an archive that had all of its
     * records deleted.  This looks a lot like an archive that has had
     * nothing done, because we would have made a "copy" list and then
     * deleted all the records, leaving us with an empty list.  (The
     * difference is that an untouched archive wouldn't have a "copy"
     * list allocated.)
     *
     * In some cases, such as doing a bulk delete that doesn't end up
     * matching anything or an attempted UpdatePresizedThread on a thread
     * that isn't actually pre-sized, we create the "copy" list but don't
     * actually change anything.  We deal with that by frying the "copy"
     * list if it doesn't have anything interesting in it (i.e. it's an
     * exact match of the "orig" list).
     */
    Nu_ResetCopySetIfUntouched(pArchive);
    if (Nu_RecordSet_IsEmpty(&pArchive->copyRecordSet) &&
        Nu_RecordSet_IsEmpty(&pArchive->newRecordSet))
    {
        if (Nu_RecordSet_GetLoaded(&pArchive->copyRecordSet)) {
            DBUG(("--- All records deleted!\n"));
            #if 0
            /*
             * Options:
             *  (1) allow it, leaving an archive with nothing but a header
             *    that will probably be rejected by other NuFX applications
             *  (2) reject it, returning an error
             *  (3) allow it, and just delete the original archive
             *
             * I dislike #1, and #3 can be implemented by the application
             * when it gets a #2.
             */
            err = kNuErrAllDeleted;
            goto bail;
            #else
            /*
             *  (4) go ahead and delete everything, then mark the archive
             *    as brand new, so that closing the archive with new
             *    records in it will trigger deletion of the archive file.
             */
            deleteAll = true;
            #endif
        } else {
            DBUG(("--- Nothing pending\n"));
            goto flushed;
        }
    }

    /* if we have any changes, we certainly should have the TOC by now */
    Assert(pArchive->haveToc);
    Assert(Nu_RecordSet_GetLoaded(&pArchive->origRecordSet));

    /*
     * Step 2: purge any records from the "copy" and "new" lists that don't
     * have any threads.  You can't delete threads from the "new" list, but
     * it's possible somebody called NuAddRecord and never put anything in it.
     */
    err = Nu_PurgeEmptyRecords(pArchive, &pArchive->copyRecordSet);
    BailError(err);
    err = Nu_PurgeEmptyRecords(pArchive, &pArchive->newRecordSet);
    BailError(err);

    /* we checked delete-all actions above, so just check for empty */
    if (Nu_RecordSet_IsEmpty(&pArchive->copyRecordSet) &&
        Nu_RecordSet_IsEmpty(&pArchive->newRecordSet) &&
        !deleteAll)
    {
        DBUG(("--- Nothing pending after purge\n"));
        goto flushed;
    }

    /*
     * Step 3: if we're in ShrinkIt-compatibility mode, add a comment
     * thread to the first record in the new list.  GSHK does this every
     * time it adds files, regardless of the prior contents of the archive.
     */
    if (pArchive->valMimicSHK) {
        err = Nu_AddCommentToFirstNewRecord(pArchive);
        BailError(err);
    }

    /*
     * Step 4: decide if we want to make changes in place, or write to
     * a temp file.  Any deletions or additions to existing records will
     * require writing to a temp file.  Additions of new records and
     * updates to pre-sized threads can be done in place.
     */
    writeToTemp = true;
    if (pArchive->valModifyOrig && Nu_NoHeavyUpdates(pArchive))
        writeToTemp = false;
    /* discard the wrapper, if desired */
    if (writeToTemp && pArchive->valDiscardWrapper)
        pArchive->headerOffset = 0;

    /*
     * Step 5: handle updates to existing records.
     */
    if (!writeToTemp) {
        /*
         * Step 5a: modifying in place, process all UPDATE ThreadMods now.
         */
        DBUG(("--- No heavy updates found, updating in place\n"));
        if (Nu_RecordSet_GetLoaded(&pArchive->copyRecordSet))
            canAbort = false;   /* modifying original, can't cleanly abort */

        err = Nu_UpdateInOriginal(pArchive);
        if (err == kNuErrDamaged)
            *pStatusFlags |= kNuFlushCorrupted;
        if (err != kNuErrNone) {
            Nu_ReportError(NU_BLOB, err, "update to original failed");
            goto bail;
        }
    } else {
        /*
         * Step 5b: not modifying in place, reconstruct the appropriate
         * parts of the original archive in the temp file, possibly copying
         * the front bits over first.  Updates and thread-adds will be
         * done here.
         */
        DBUG(("--- Updating to temp file (valModifyOrig=%ld)\n",
            pArchive->valModifyOrig));
        err = Nu_CreateTempFromOriginal(pArchive);
        if (err != kNuErrNone) {
            DBUG(("--- Create temp from original failed\n"));
            goto bail;
        }
    }
    /* on completion, tmpFp (or archiveFp) points to current archive EOF */

    /*
     * Step 6: add the new records from the "new" list, if any.  Add a
     * filename thread to records where one wasn't provided.  These records
     * are either added to the original archive or the temp file as
     * appropriate.
     */
    if (writeToTemp)
        err = Nu_CreateNewRecords(pArchive, pArchive->tmpFp);
    else
        err = Nu_CreateNewRecords(pArchive, pArchive->archiveFp);
    BailError(err);

    /* on completion, tmpFp (or archiveFp) points to current archive EOF */

    /*
     * Step 7: truncate the archive.  This isn't strictly necessary.  It
     * comes in handy if we were compressing the very last file and it
     * actually expanded.  We went back and wrote the uncompressed data,
     * but there's a bunch of junk after it from the first try.
     *
     * On systems like Win32 that don't support ftruncate, this will fail,
     * so we just ignore the result.
     */
    if (writeToTemp) {
        err = Nu_FTell(pArchive->tmpFp, &finalOffset);
        BailError(err);
        (void) Nu_TruncateOpenFile(pArchive->tmpFp, finalOffset);
    } else {
        err = Nu_FTell(pArchive->archiveFp, &finalOffset);
        BailError(err);
        (void) Nu_TruncateOpenFile(pArchive->archiveFp, finalOffset);
    }

    /*
     * Step 8: create an updated master header, and write it to the
     * appropriate file.  The "newMasterHeader" field in pArchive will
     * hold the new header.
     */
    Assert(!pArchive->newMasterHeader.isValid);
    if (writeToTemp) {
        err = Nu_FSeek(pArchive->tmpFp, pArchive->headerOffset, SEEK_SET);
        BailError(err);
        err = Nu_UpdateMasterHeader(pArchive, pArchive->tmpFp,
                finalOffset - pArchive->headerOffset);
        /* fall through with err */
    } else {
        err = Nu_FSeek(pArchive->archiveFp, pArchive->headerOffset, SEEK_SET);
        BailError(err);
        err = Nu_UpdateMasterHeader(pArchive, pArchive->archiveFp,
                finalOffset - pArchive->headerOffset);
        /* fall through with err */
    }
    if (err == kNuErrNoRecords && !deleteAll) {
        /*
         * Somehow we ended up without any records at all.  If we managed
         * to get this far, it could only be because the user told us to
         * skip adding everything.
         */
        Nu_ReportError(NU_BLOB, kNuErrNone, "no records in this archive");
        goto bail;
    } else if (err != kNuErrNone) {
        Nu_ReportError(NU_BLOB, err, "failed writing master header");
        goto bail;
    }
    Assert(pArchive->newMasterHeader.isValid);


    /*
     * Step 9: carry forward the BXY, SEA, or BSE header, if necessary.  This
     * implicitly assumes that the header doesn't change size.  If this
     * assumption is invalid, we'd need to adjust "headerOffset" earlier,
     * or do lots of data copying.  Looks like Binary II and SEA headers
     * are both fixed size, so we should be okay.
     *
     * We also carry forward any unrecognized junk.
     */
    if (pArchive->headerOffset) {
        if (writeToTemp) {
            if (!pArchive->valDiscardWrapper) {
                DBUG(("--- Preserving wrapper\n"));
                /* copy header to temp */
                err = Nu_CopyWrapperToTemp(pArchive);
                BailError(err);
                /* update fields that require it */
                err = Nu_UpdateWrapper(pArchive, pArchive->tmpFp);
                BailError(err);
                /* check the padding */
                err = Nu_AdjustWrapperPadding(pArchive, pArchive->tmpFp);
                BailError(err);
            }
        } else {
            /* may need to tweak what's in place? */
            DBUG(("--- Updating wrapper\n"));
            err = Nu_UpdateWrapper(pArchive, pArchive->archiveFp);
            BailError(err);
            /* should only be necessary if we've added new records */
            err = Nu_AdjustWrapperPadding(pArchive, pArchive->archiveFp);
            BailError(err);
        }
    }

    /*
     * Step 10: if necessary, remove the original file and rename the
     * temp file over it.
     *
     * I'm not messing with access permissions on the archive file here,
     * because if they opened it read-write then the archive itself
     * must also be read-write (unless somebody snuck in and chmodded it
     * while we were busy).  The temp file is certainly writable, so we
     * should be able to just leave it all alone.
     *
     * I'm closing both temp and archive before renaming, because on some
     * operating systems you can't do certain things with open files.
     */
    if (writeToTemp) {
        canAbort = false;   /* no going back */
        *pStatusFlags |= kNuFlushSucceeded;     /* temp file is fully valid */

        fclose(pArchive->archiveFp);
        pArchive->archiveFp = NULL;

        err = Nu_DeleteArchiveFile(pArchive);
        if (err != kNuErrNone) {
            Nu_ReportError(NU_BLOB, err, "unable to remove original archive");
            Nu_ReportError(NU_BLOB, kNuErrNone, "New data is in '%s'",
                pArchive->tmpPathnameUNI);
            *pStatusFlags |= kNuFlushInaccessible;
            goto bail_reopen;       /* must re-open archiveFp */
        }

        fclose(pArchive->tmpFp);
        pArchive->tmpFp = NULL;

        err = Nu_RenameTempToArchive(pArchive);
        if (err != kNuErrNone) {
            Nu_ReportError(NU_BLOB, err, "unable to rename temp file");
            Nu_ReportError(NU_BLOB, kNuErrNone,
                "NOTE: only copy of archive is in '%s'",
                pArchive->tmpPathnameUNI);
            /* maintain Entry.c semantics (and keep them from removing temp) */
            Nu_Free(pArchive, pArchive->archivePathnameUNI);
            pArchive->archivePathnameUNI = NULL;
            Nu_Free(pArchive, pArchive->tmpPathnameUNI);
            pArchive->tmpPathnameUNI = NULL;
            /* bail will put us into read-only mode, which is what we want */
            goto bail;
        }

bail_reopen:
        pArchive->archiveFp = fopen(pArchive->archivePathnameUNI,
                                kNuFileOpenReadWrite);
        if (pArchive->archiveFp == NULL) {
            err = errno ? errno : -1;
            Nu_ReportError(NU_BLOB, err,
                "unable to reopen archive file '%s' after rename",
                pArchive->archivePathnameUNI);
            *pStatusFlags |= kNuFlushInaccessible;
            goto bail;      /* the Entry.c funcs will obstruct further use */
        }

        if (err != kNuErrNone)  // earlier failure?
            goto bail;
    } else {
        fflush(pArchive->archiveFp);
        if (ferror(pArchive->archiveFp)) {
            err = kNuErrFileWrite;
            Nu_ReportError(NU_BLOB, kNuErrNone, "final archive flush failed");
            *pStatusFlags |= kNuFlushCorrupted;
            goto bail;
        }
        canAbort = false;
        *pStatusFlags |= kNuFlushSucceeded;
    }

    Assert(canAbort == false);

    /*
     * Step 11: clean up data structures.  If we have a "copy" list, then
     * throw out the "orig" list and move the "copy" list over it.  Append
     * anything in the "new" list to it.  Move the "new" master header
     * over the original.
     */
    Assert(pArchive->newMasterHeader.isValid);
    Nu_MasterHeaderCopy(pArchive, &pArchive->masterHeader,
        &pArchive->newMasterHeader);
    if (Nu_RecordSet_GetLoaded(&pArchive->copyRecordSet)) {
        err = Nu_RecordSet_FreeAllRecords(pArchive, &pArchive->origRecordSet);
        BailError(err);
        err = Nu_RecordSet_MoveAllRecords(pArchive, &pArchive->origRecordSet,
                &pArchive->copyRecordSet);
        BailError(err);
    }
    err = Nu_RecordSet_MoveAllRecords(pArchive, &pArchive->origRecordSet,
            &pArchive->newRecordSet);
    BailError(err);
    err = Nu_ResetUsedFlags(pArchive, &pArchive->origRecordSet);
    BailError(err);

flushed:
    /*
     * Step 12: reset the "copy" and "new" lists, and reset the temp file.
     * Clear out the "new" master header copy.
     */
    err = Nu_RecordSet_FreeAllRecords(pArchive, &pArchive->copyRecordSet);
    BailError(err);
    err = Nu_RecordSet_FreeAllRecords(pArchive, &pArchive->newRecordSet);
    BailError(err);
    pArchive->newMasterHeader.isValid = false;

    err = Nu_ResetTempFile(pArchive);
    if (err != kNuErrNone) {
        /* can't NuAbort() our way out of a bad temp file */
        canAbort = false;
        goto bail;
    }

    if (deleteAll) {
        /* there's nothing in it, so treat it like a newly-created archive */
        /* (that way it gets deleted if the app closes without adding stuff) */
        DBUG(("--- marking archive as newly created\n"));
        pArchive->newlyCreated = true;
        /*pArchive->valModifyOrig = true;*/
    }

bail:
    if (err != kNuErrNone) {
        if (canAbort) {
            (void) Nu_Abort(pArchive);
            Assert(!(*pStatusFlags & kNuFlushSucceeded));
            *pStatusFlags |= kNuFlushAborted;

            /*
             * If we were adding to original archive, truncate it back if
             * we are able to do so.  This retains any BXY/BSE wrapper padding.
             */
            if (!writeToTemp) {
                NuError err2;
                err2 = Nu_TruncateOpenFile(pArchive->archiveFp, initialEOF);
                if (err2 == kNuErrNone) {
                    DBUG(("+++ truncated orig archive back to %ld\n",
                        initialEOF));
                } else {
                    DBUG(("+++ truncate orig failed (err=%d)\n", err2));
                }
            }
        } else {
            Nu_ReportError(NU_BLOB, kNuErrNone,
                "disabling write access after failed update");
            pArchive->openMode = kNuOpenRO;
            *pStatusFlags |= kNuFlushReadOnly;
        }
    }

    /* last-minute sanity check */
    Assert(pArchive->origRecordSet.numRecords == 0 ||
        (pArchive->origRecordSet.nuRecordHead != NULL &&
         pArchive->origRecordSet.nuRecordTail != NULL));

    return err;
}


/*
 * Abort any pending changes.
 */
NuError Nu_Abort(NuArchive* pArchive)
{
    Assert(pArchive != NULL);

    if (Nu_IsReadOnly(pArchive))
        return kNuErrArchiveRO;

    DBUG(("--- Aborting changes\n"));

    /*
     * Throw out the "copy" and "new" record sets, and reset the
     * temp file.
     */
    (void) Nu_RecordSet_FreeAllRecords(pArchive, &pArchive->copyRecordSet);
    (void) Nu_RecordSet_FreeAllRecords(pArchive, &pArchive->newRecordSet);
    pArchive->newMasterHeader.isValid = false;

    return Nu_ResetTempFile(pArchive);
}

