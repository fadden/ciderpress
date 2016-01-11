/*
 * NuFX archive manipulation library
 * Copyright (C) 2000-2007 by Andy McFadden, All Rights Reserved.
 * This is free software; you can redistribute it and/or modify it under the
 * terms of the BSD License, see the file COPYING-LIB.
 *
 * Record-level operations.
 */
#include "NufxLibPriv.h"


/*
 * Local constants.
 */
static const uint8_t kNufxID[kNufxIDLen] = { 0x4e, 0xf5, 0x46, 0xd8 };


/*
 * ===========================================================================
 *      Simple NuRecord stuff
 * ===========================================================================
 */

/*
 * Initialize the contents of a NuRecord.  The goal here is to init the
 * things that a Nu_FreeRecordContents call will check, so that we don't
 * end up trying to free garbage.  No need to memset() the whole thing.
 */
static NuError Nu_InitRecordContents(NuArchive* pArchive, NuRecord* pRecord)
{
    Assert(pRecord != NULL);

    DebugFill(pRecord, sizeof(*pRecord));

    pRecord->recOptionList = NULL;
    pRecord->extraBytes = NULL;
    pRecord->recFilenameMOR = NULL;
    pRecord->threadFilenameMOR = NULL;
    pRecord->newFilenameMOR = NULL;
    pRecord->pThreads = NULL;
    pRecord->pNext = NULL;
    pRecord->pThreadMods = NULL;
    pRecord->dirtyHeader = false;
    pRecord->dropRecFilename = false;
    pRecord->isBadMac = false;

    return kNuErrNone;
}

/*
 * Allocate and initialize a new NuRecord struct.
 */
static NuError Nu_RecordNew(NuArchive* pArchive, NuRecord** ppRecord)
{
    Assert(ppRecord != NULL);

    *ppRecord = Nu_Malloc(pArchive, sizeof(**ppRecord));
    if (*ppRecord == NULL)
        return kNuErrMalloc;

    return Nu_InitRecordContents(pArchive, *ppRecord);
}

/*
 * Free anything allocated within a record.  Doesn't try to free the record
 * itself.
 */
static NuError Nu_FreeRecordContents(NuArchive* pArchive, NuRecord* pRecord)
{
    Assert(pRecord != NULL);

    Nu_Free(pArchive, pRecord->recOptionList);
    Nu_Free(pArchive, pRecord->extraBytes);
    Nu_Free(pArchive, pRecord->recFilenameMOR);
    Nu_Free(pArchive, pRecord->threadFilenameMOR);
    Nu_Free(pArchive, pRecord->newFilenameMOR);
    Nu_Free(pArchive, pRecord->pThreads);
    /* don't Free(pRecord->pNext)! */
    Nu_FreeThreadMods(pArchive, pRecord);

    (void) Nu_InitRecordContents(pArchive, pRecord);    /* mark as freed */

    return kNuErrNone;
}

/*
 * Free up a NuRecord struct.
 */
static NuError Nu_RecordFree(NuArchive* pArchive, NuRecord* pRecord)
{
    if (pRecord == NULL)
        return kNuErrNone;

    (void) Nu_FreeRecordContents(pArchive, pRecord);
    Nu_Free(pArchive, pRecord);

    return kNuErrNone;
}

/*
 * Copy a field comprised of a buffer and a length from one structure to
 * another.  It is assumed that the length value has already been copied.
 */
static NuError CopySizedField(NuArchive* pArchive, void* vppDst,
    const void* vpSrc, uint32_t len)
{
    NuError err = kNuErrNone;
    uint8_t** ppDst = vppDst;
    const uint8_t* pSrc = vpSrc;

    Assert(ppDst != NULL);

    if (len) {
        Assert(pSrc != NULL);
        *ppDst = Nu_Malloc(pArchive, len);
        BailAlloc(*ppDst);
        memcpy(*ppDst, pSrc, len);
    } else {
        Assert(pSrc == NULL);
        *ppDst = NULL;
    }

bail:
    return err;
}

/*
 * Make a copy of a record.
 */
static NuError Nu_RecordCopy(NuArchive* pArchive, NuRecord** ppDst,
    const NuRecord* pSrc)
{
    NuError err;
    NuRecord* pDst;

    err = Nu_RecordNew(pArchive, ppDst);
    BailError(err);

    /* copy all the static fields, then copy or blank the "hairy" parts */
    pDst = *ppDst;
    memcpy(pDst, pSrc, sizeof(*pSrc));
    CopySizedField(pArchive, &pDst->recOptionList, pSrc->recOptionList,
        pSrc->recOptionSize);
    CopySizedField(pArchive, &pDst->extraBytes, pSrc->extraBytes,
        pSrc->extraCount);
    CopySizedField(pArchive, &pDst->recFilenameMOR, pSrc->recFilenameMOR,
        pSrc->recFilenameLength == 0 ? 0 : pSrc->recFilenameLength+1);
    CopySizedField(pArchive, &pDst->threadFilenameMOR, pSrc->threadFilenameMOR,
        pSrc->threadFilenameMOR == NULL ? 0 : strlen(pSrc->threadFilenameMOR) +1);
    CopySizedField(pArchive, &pDst->newFilenameMOR, pSrc->newFilenameMOR,
        pSrc->newFilenameMOR == NULL ? 0 : strlen(pSrc->newFilenameMOR) +1);
    CopySizedField(pArchive, &pDst->pThreads, pSrc->pThreads,
        pSrc->recTotalThreads * sizeof(*pDst->pThreads));

    /* now figure out what the filename is supposed to point at */
    if (pSrc->filenameMOR == pSrc->threadFilenameMOR)
        pDst->filenameMOR = pDst->threadFilenameMOR;
    else if (pSrc->filenameMOR == pSrc->recFilenameMOR)
        pDst->filenameMOR = pDst->recFilenameMOR;
    else if (pSrc->filenameMOR == pSrc->newFilenameMOR)
        pDst->filenameMOR = pDst->newFilenameMOR;
    else
        pDst->filenameMOR = pSrc->filenameMOR; /* probably static kDefault value */

    pDst->pNext = NULL;

    /* these only hold for copy from orig... may need to remove */
    Assert(pSrc->pThreadMods == NULL);
    Assert(!pSrc->dirtyHeader);

bail:
    return err;
}


/*
 * Add a ThreadMod to the list in the NuRecord.
 *
 * In general, the order is not significant.  However, if we're adding
 * a bunch of "add" threadMods for control threads to a record, their
 * order might be important.  So, we want to add the threadMod to the
 * end of the list.
 *
 * I'm expecting these lists to be short, so walking down them is
 * acceptable.  We could do simple optimizations, like only preserving
 * ordering for "add" threadMods, but even that seems silly.
 */
void Nu_RecordAddThreadMod(NuRecord* pRecord, NuThreadMod* pThreadMod)
{
    NuThreadMod* pScanThreadMod;

    Assert(pRecord != NULL);
    Assert(pThreadMod != NULL);

    if (pRecord->pThreadMods == NULL) {
        pRecord->pThreadMods = pThreadMod;
    } else {
        pScanThreadMod = pRecord->pThreadMods;
        while (pScanThreadMod->pNext != NULL)
            pScanThreadMod = pScanThreadMod->pNext;

        pScanThreadMod->pNext = pThreadMod;
    }

    pThreadMod->pNext = NULL;
}


/*
 * Decide if a record is empty.  An empty record is one that will have no
 * threads after all adds and deletes are processed.
 *
 * You can't delete something you just added or has been updated, and you
 * can't update something that has been deleted, so any "add" or "update"
 * items indicate that the thread isn't empty.
 *
 * You can't delete a thread more than once, or delete a thread that
 * doesn't exist, so all we need to do is count up the number of current
 * threads, subtract the number of deletes, and return "true" if the net
 * result is zero.
 */
Boolean Nu_RecordIsEmpty(NuArchive* pArchive, const NuRecord* pRecord)
{
    const NuThreadMod* pThreadMod;
    int numThreads;

    Assert(pRecord != NULL);

    numThreads = pRecord->recTotalThreads;

    pThreadMod = pRecord->pThreadMods;
    while (pThreadMod != NULL) {
        switch (pThreadMod->entry.kind) {
        case kNuThreadModAdd:
        case kNuThreadModUpdate:
            return false;
        case kNuThreadModDelete:
            numThreads--;
            break;
        case kNuThreadModUnknown:
        default:
            Assert(0);
            return false;
        }

        pThreadMod = pThreadMod->pNext;
    }

    if (numThreads > 0)
        return false;
    else if (numThreads == 0)
        return true;
    else {
        Assert(0);
        Nu_ReportError(NU_BLOB, kNuErrInternal,
            "Thread counting failed (%d)", numThreads);
        return false;
    }
}


/*
 * ===========================================================================
 *      NuRecordSet functions
 * ===========================================================================
 */

/*
 * Trivial getters and setters
 */

Boolean Nu_RecordSet_GetLoaded(const NuRecordSet* pRecordSet)
{
    Assert(pRecordSet != NULL);
    return pRecordSet->loaded;
}

void Nu_RecordSet_SetLoaded(NuRecordSet* pRecordSet, Boolean val)
{
    pRecordSet->loaded = val;
}

uint32_t Nu_RecordSet_GetNumRecords(const NuRecordSet* pRecordSet)
{
    return pRecordSet->numRecords;
}

void Nu_RecordSet_SetNumRecords(NuRecordSet* pRecordSet, uint32_t val)
{
    pRecordSet->numRecords = val;
}

void Nu_RecordSet_IncNumRecords(NuRecordSet* pRecordSet)
{
    pRecordSet->numRecords++;
}

NuRecord* Nu_RecordSet_GetListHead(const NuRecordSet* pRecordSet)
{
    return pRecordSet->nuRecordHead;
}

NuRecord** Nu_RecordSet_GetListHeadPtr(NuRecordSet* pRecordSet)
{
    return &pRecordSet->nuRecordHead;
}

NuRecord* Nu_RecordSet_GetListTail(const NuRecordSet* pRecordSet)
{
    return pRecordSet->nuRecordTail;
}


/*
 * Returns "true" if the record set has no records or hasn't ever been
 * used.
 */
Boolean Nu_RecordSet_IsEmpty(const NuRecordSet* pRecordSet)
{
    if (!pRecordSet->loaded || pRecordSet->numRecords == 0)
        return true;

    return false;
}

/*
 * Free the list of records, and reset the record sets to initial state.
 */
NuError Nu_RecordSet_FreeAllRecords(NuArchive* pArchive,
    NuRecordSet* pRecordSet)
{
    NuError err = kNuErrNone;
    NuRecord* pRecord;
    NuRecord* pNextRecord;

    if (!pRecordSet->loaded) {
        Assert(pRecordSet->nuRecordHead == NULL);
        Assert(pRecordSet->nuRecordTail == NULL);
        Assert(pRecordSet->numRecords == 0);
        return kNuErrNone;
    }

    DBUG(("+++ FreeAllRecords\n"));
    pRecord = pRecordSet->nuRecordHead;
    while (pRecord != NULL) {
        pNextRecord = pRecord->pNext;

        err = Nu_RecordFree(pArchive, pRecord);
        BailError(err);     /* don't really expect this to fail */

        pRecord = pNextRecord;
    }

    pRecordSet->nuRecordHead = pRecordSet->nuRecordTail = NULL;
    pRecordSet->numRecords = 0;
    pRecordSet->loaded = false;

bail:
    return err;
}


/*
 * Add a new record to the end of the list.
 */
static NuError Nu_RecordSet_AddRecord(NuRecordSet* pRecordSet,
    NuRecord* pRecord)
{
    Assert(pRecordSet != NULL);
    Assert(pRecord != NULL);

    /* if one is NULL, both must be NULL */
    Assert(pRecordSet->nuRecordHead == NULL || pRecordSet->nuRecordTail != NULL);
    Assert(pRecordSet->nuRecordTail == NULL || pRecordSet->nuRecordHead != NULL);

    if (pRecordSet->nuRecordHead == NULL) {
        /* empty list */
        pRecordSet->nuRecordHead = pRecordSet->nuRecordTail = pRecord;
        pRecordSet->loaded = true;
        Assert(!pRecordSet->numRecords);
    } else {
        pRecord->pNext = NULL;
        pRecordSet->nuRecordTail->pNext = pRecord;
        pRecordSet->nuRecordTail = pRecord;
    }

    pRecordSet->numRecords++;

    return kNuErrNone;
}


/*
 * Delete a record from the record set.  Pass in a pointer to the pointer
 * to the record (usually either the head pointer or another record's
 * "pNext" pointer).
 *
 * (Should have a "heavy assert" mode where we verify that "ppRecord"
 * actually has something to do with pRecordSet.)
 */
NuError Nu_RecordSet_DeleteRecordPtr(NuArchive* pArchive,
    NuRecordSet* pRecordSet, NuRecord** ppRecord)
{
    NuError err;
    NuRecord* pRecord;

    Assert(pRecordSet != NULL);
    Assert(ppRecord != NULL);
    Assert(*ppRecord != NULL);

    /* save a copy of the record we're freeing */
    pRecord = *ppRecord;

    /* update the pHead or pNext pointer */
    *ppRecord = (*ppRecord)->pNext;
    pRecordSet->numRecords--;

    /* if we're deleting the tail, we have to find the "new" last entry */
    if (pRecord == pRecordSet->nuRecordTail) {
        if (pRecordSet->nuRecordHead == NULL) {
            /* this was the last entry; we're done */
            pRecordSet->nuRecordTail = NULL;
        } else {
            /* walk through the list... delete bottom-up will be slow! */
            pRecordSet->nuRecordTail = pRecordSet->nuRecordHead;
            while (pRecordSet->nuRecordTail->pNext != NULL)
                pRecordSet->nuRecordTail = pRecordSet->nuRecordTail->pNext;
        }
    }

    if (pRecordSet->numRecords)
        Assert(pRecordSet->nuRecordHead!=NULL && pRecordSet->nuRecordTail!=NULL);
    else
        Assert(pRecordSet->nuRecordHead==NULL && pRecordSet->nuRecordTail==NULL);

    err = Nu_RecordFree(pArchive, pRecord);
    return err;
}

/*
 * Delete a record from the record set.
 */
NuError Nu_RecordSet_DeleteRecord(NuArchive* pArchive, NuRecordSet* pRecordSet,
    NuRecord* pRecord)
{
    NuError err;
    NuRecord** ppRecord;

    ppRecord = Nu_RecordSet_GetListHeadPtr(pRecordSet);
    Assert(ppRecord != NULL);
    Assert(*ppRecord != NULL);

    /* look for the record, so we can update his neighbors */
    /* (this also ensures that the record really is in the set we think it is)*/
    while (*ppRecord) {
        if (*ppRecord == pRecord) {
            err = Nu_RecordSet_DeleteRecordPtr(pArchive, pRecordSet, ppRecord);
            BailError(err);
            goto bail;
        }

        ppRecord = &((*ppRecord)->pNext);
    }

    DBUG(("--- Nu_RecordSet_DeleteRecord failed\n"));
    err = kNuErrNotFound;

bail:
    return err;
}

/*
 * Make a clone of a record set.  This is used to create the "copy" record
 * set out of the "orig" set.
 */
NuError Nu_RecordSet_Clone(NuArchive* pArchive, NuRecordSet* pDstSet,
    const NuRecordSet* pSrcSet)
{
    NuError err = kNuErrNone;
    const NuRecord* pSrcRecord;
    NuRecord* pDstRecord;

    Assert(pDstSet != NULL);
    Assert(pSrcSet != NULL);
    Assert(Nu_RecordSet_GetLoaded(pDstSet) == false);
    Assert(Nu_RecordSet_GetLoaded(pSrcSet) == true);

    DBUG(("--- Cloning record set\n"));

    Nu_RecordSet_SetLoaded(pDstSet, true);

    /* copy each record over */
    pSrcRecord = pSrcSet->nuRecordHead;
    while (pSrcRecord != NULL) {
        err = Nu_RecordCopy(pArchive, &pDstRecord, pSrcRecord);
        BailError(err);
        err = Nu_RecordSet_AddRecord(pDstSet, pDstRecord);
        BailError(err);

        pSrcRecord = pSrcRecord->pNext;
    }

    Assert(pDstSet->numRecords == pSrcSet->numRecords);

bail:
    if (err != kNuErrNone) {
        Nu_RecordSet_FreeAllRecords(pArchive, pDstSet);
    }
    return err;
}

/*
 * Move all of the records from one record set to another.  The records
 * from "pSrcSet" are appended to "pDstSet".
 *
 * On completion, "pSrcSet" will be empty and "unloaded".
 */
NuError Nu_RecordSet_MoveAllRecords(NuArchive* pArchive, NuRecordSet* pDstSet,
    NuRecordSet* pSrcSet)
{
    NuError err = kNuErrNone;

    Assert(pDstSet != NULL);
    Assert(pSrcSet != NULL);

    /* move records over */
    if (Nu_RecordSet_GetNumRecords(pSrcSet)) {
        Assert(pSrcSet->loaded);
        Assert(pSrcSet->nuRecordHead != NULL);
        Assert(pSrcSet->nuRecordTail != NULL);
        if (pDstSet->nuRecordHead == NULL) {
            /* empty dst list */
            Assert(pDstSet->nuRecordTail == NULL);
            pDstSet->nuRecordHead = pSrcSet->nuRecordHead;
            pDstSet->nuRecordTail = pSrcSet->nuRecordTail;
            pDstSet->numRecords = pSrcSet->numRecords;
            pDstSet->loaded = true;
        } else {
            /* append to dst list */
            Assert(pDstSet->loaded);
            Assert(pDstSet->nuRecordTail != NULL);
            pDstSet->nuRecordTail->pNext = pSrcSet->nuRecordHead;
            pDstSet->nuRecordTail = pSrcSet->nuRecordTail;
            pDstSet->numRecords += pSrcSet->numRecords;
        }
    } else {
        /* no records in src set */
        Assert(pSrcSet->nuRecordHead == NULL);
        Assert(pSrcSet->nuRecordTail == NULL);

        if (pSrcSet->loaded)
            pDstSet->loaded = true;
    }

    /* nuke all pointers in original list */
    pSrcSet->nuRecordHead = pSrcSet->nuRecordTail = NULL;
    pSrcSet->numRecords = 0;
    pSrcSet->loaded = false;

    return err;
}


/*
 * Find a record in the list by index.
 */
NuError Nu_RecordSet_FindByIdx(const NuRecordSet* pRecordSet,
    NuRecordIdx recIdx, NuRecord** ppRecord)
{
    NuRecord* pRecord;

    pRecord = pRecordSet->nuRecordHead;
    while (pRecord != NULL) {
        if (pRecord->recordIdx == recIdx) {
            *ppRecord = pRecord;
            return kNuErrNone;
        }

        pRecord = pRecord->pNext;
    }

    return kNuErrRecIdxNotFound;
}


/*
 * Search for a specific thread in all records in the specified record set.
 */
NuError Nu_RecordSet_FindByThreadIdx(NuRecordSet* pRecordSet,
    NuThreadIdx threadIdx, NuRecord** ppRecord, NuThread** ppThread)
{
    NuError err = kNuErrThreadIdxNotFound;
    NuRecord* pRecord;

    pRecord = Nu_RecordSet_GetListHead(pRecordSet);
    while (pRecord != NULL) {
        err = Nu_FindThreadByIdx(pRecord, threadIdx, ppThread);
        if (err == kNuErrNone) {
            *ppRecord = pRecord;
            break;
        }
        pRecord = pRecord->pNext;
    }

    Assert(err != kNuErrNone || (*ppRecord != NULL && *ppThread != NULL));
    return err;
}


/*
 * Compare two record filenames.  This comes into play when looking for
 * conflicts while adding records to an archive.
 *
 * Interesting issues:
 *  - some filesystems are case-sensitive, some aren't
 *  - the fssep may be different ('/', ':') for otherwise equivalent names
 *  - system-dependent conversions could resolve two different names to
 *    the same thing
 *
 * Some of these are out of our control.  For now, I'm just doing a
 * case-insensitive comparison, since the most interesting case for us is
 * when the person is adding a data fork and a resource fork from the
 * same file during the same operation.
 *
 * [ Could run both names through the pathname conversion callback first?
 *   Might be expensive. ]
 *
 * Returns an integer greater than, equal to, or less than 0, if the
 * string pointed to by name1 is greater than, equal to, or less than
 * the string pointed to by s2, respectively (i.e. same as strcmp).
 */
static int Nu_CompareRecordNames(const char* name1MOR, const char* name2MOR)
{
#ifdef NU_CASE_SENSITIVE
    return strcmp(name1MOR, name2MOR);
#else
    return strcasecmp(name1MOR, name2MOR);
#endif
}


/*
 * Find a record in the list by storageName.
 */
static NuError Nu_RecordSet_FindByName(const NuRecordSet* pRecordSet,
    const char* nameMOR, NuRecord** ppRecord)
{
    NuRecord* pRecord;

    Assert(pRecordSet != NULL);
    Assert(pRecordSet->loaded);
    Assert(nameMOR != NULL);
    Assert(ppRecord != NULL);

    pRecord = pRecordSet->nuRecordHead;
    while (pRecord != NULL) {
        if (Nu_CompareRecordNames(pRecord->filenameMOR, nameMOR) == 0) {
            *ppRecord = pRecord;
            return kNuErrNone;
        }

        pRecord = pRecord->pNext;
    }

    return kNuErrRecNameNotFound;
}

/*
 * Find a record in the list by storageName, starting from the end and
 * searching backwards.
 *
 * Since we don't actually have a "prev" pointer in the record, we end
 * up scanning the entire list and keeping the last match.  If this
 * causes a notable reduction in efficiency we'll have to fix this.
 */
static NuError Nu_RecordSet_ReverseFindByName(const NuRecordSet* pRecordSet,
    const char* nameMOR, NuRecord** ppRecord)
{
    NuRecord* pRecord;
    NuRecord* pFoundRecord = NULL;

    Assert(pRecordSet != NULL);
    Assert(pRecordSet->loaded);
    Assert(nameMOR != NULL);
    Assert(ppRecord != NULL);

    pRecord = pRecordSet->nuRecordHead;
    while (pRecord != NULL) {
        if (Nu_CompareRecordNames(pRecord->filenameMOR, nameMOR) == 0)
            pFoundRecord = pRecord;

        pRecord = pRecord->pNext;
    }

    if (pFoundRecord != NULL) {
        *ppRecord = pFoundRecord;
        return kNuErrNone;
    }
    return kNuErrRecNameNotFound;
}


/*
 * We have a copy of the record in the "copy" set, but we've decided
 * (perhaps because the user elected to Skip a failed add) that we'd
 * rather have the original.
 *
 * Delete the record from the "copy" set, clone the "orig" record, and
 * insert the "orig" record into the same spot in the "copy" set.
 *
 * "ppNewRecord" will get a pointer to the newly-created clone.
 */
NuError Nu_RecordSet_ReplaceRecord(NuArchive* pArchive, NuRecordSet* pBadSet,
    NuRecord* pBadRecord, NuRecordSet* pGoodSet, NuRecord** ppNewRecord)
{
    NuError err;
    NuRecord* pGoodRecord;
    NuRecord* pSiblingRecord;
    NuRecord* pNewRecord = NULL;

    Assert(pArchive != NULL);
    Assert(pBadSet != NULL);
    Assert(pBadRecord != NULL);
    Assert(pGoodSet != NULL);
    Assert(ppNewRecord != NULL);

    /*
     * Find a record in "pGoodSet" that has the same record index as
     * the "bad" record.
     */
    err = Nu_RecordSet_FindByIdx(pGoodSet, pBadRecord->recordIdx,
            &pGoodRecord);
    BailError(err);

    /*
     * Clone the original.
     */
    err = Nu_RecordCopy(pArchive, &pNewRecord, pGoodRecord);
    BailError(err);

    /*
     * Insert the new one into the "bad" record set, in the exact same
     * position.
     */
    pNewRecord->pNext = pBadRecord->pNext;
    if (pBadSet->nuRecordTail == pBadRecord)
        pBadSet->nuRecordTail = pNewRecord;
    if (pBadSet->nuRecordHead == pBadRecord)
        pBadSet->nuRecordHead = pNewRecord;
    else {
        /* find the record that points to pBadRecord */
        pSiblingRecord = pBadSet->nuRecordHead;
        while (pSiblingRecord->pNext != pBadRecord && pSiblingRecord != NULL)
            pSiblingRecord = pSiblingRecord->pNext;

        if (pSiblingRecord == NULL) {
            /* looks like "pBadRecord" wasn't part of "pBadSet" after all */
            Assert(0);
            err = kNuErrInternal;
            goto bail;
        }

        pSiblingRecord->pNext = pNewRecord;
    }

    err = Nu_RecordFree(pArchive, pBadRecord);
    BailError(err);

    *ppNewRecord = pNewRecord;
    pNewRecord = NULL;   /* don't free */

bail:
    if (pNewRecord != NULL)
        Nu_RecordFree(pArchive, pNewRecord);
    return err;
}


/*
 * ===========================================================================
 *      Assorted utility functions
 * ===========================================================================
 */

/*
 * Ask the user if it's okay to ignore a bad CRC.  If we can't ask the
 * user, return "false".
 */
Boolean Nu_ShouldIgnoreBadCRC(NuArchive* pArchive, const NuRecord* pRecord,
    NuError err)
{
    NuErrorStatus errorStatus;
    NuResult result;
    Boolean retval = false;
    UNICHAR* pathnameUNI = NULL;

    Assert(pArchive->valIgnoreCRC == false);

    if (pArchive->errorHandlerFunc != NULL) {
        errorStatus.operation = kNuOpTest;      /* mostly accurate */
        errorStatus.err = err;
        errorStatus.sysErr = 0;
        errorStatus.message = NULL;
        errorStatus.pRecord = pRecord;
        errorStatus.pathnameUNI = NULL;
        errorStatus.origPathname = NULL;
        errorStatus.filenameSeparator = 0;
        if (pRecord != NULL) {
            pathnameUNI = Nu_CopyMORToUNI(pRecord->filenameMOR);
            errorStatus.pathnameUNI = pathnameUNI;
            errorStatus.filenameSeparator =
                NuGetSepFromSysInfo(pRecord->recFileSysInfo);
        }
        /*errorStatus.origArchiveTouched = false;*/
        errorStatus.canAbort = true;
        errorStatus.canRetry = false;
        errorStatus.canIgnore = true;
        errorStatus.canSkip = false;
        errorStatus.canRename = false;
        errorStatus.canOverwrite = false;

        result = (*pArchive->errorHandlerFunc)(pArchive, &errorStatus);

        switch (result) {
        case kNuAbort:
            goto bail;
        case kNuIgnore:
            retval = true;
            goto bail;
        case kNuSkip:
        case kNuOverwrite:
        case kNuRetry:
        case kNuRename:
        default:
            Nu_ReportError(NU_BLOB, kNuErrSyntax,
                "Wasn't expecting result %d here", result);
            break;
        }
    }

bail:
    Nu_Free(pArchive, pathnameUNI);
    return retval;
}


/*
 * Read the next NuFX record from the current offset in the archive stream.
 * This includes the record header and the thread header blocks.
 *
 * Pass in a NuRecord structure that will hold the data we read.
 */
static NuError Nu_ReadRecordHeader(NuArchive* pArchive, NuRecord* pRecord)
{
    NuError err = kNuErrNone;
    uint16_t crc;
    FILE* fp;
    int bytesRead;

    Assert(pArchive != NULL);
    Assert(pRecord != NULL);
    Assert(pRecord->pThreads == NULL);
    Assert(pRecord->pNext == NULL);

    fp = pArchive->archiveFp;

    pRecord->recordIdx = Nu_GetNextRecordIdx(pArchive);

    /* points to whichever filename storage we like best */
    pRecord->filenameMOR = NULL;
    pRecord->fileOffset = pArchive->currentOffset;

    (void) Nu_ReadBytes(pArchive, fp, pRecord->recNufxID, kNufxIDLen);
    if (memcmp(kNufxID, pRecord->recNufxID, kNufxIDLen) != 0) {
        err = kNuErrRecHdrNotFound;
        Nu_ReportError(NU_BLOB, kNuErrNone,
            "Couldn't find start of next record");
        goto bail;
    }

    /*
     * Read the static fields.
     */
    crc = 0;
    pRecord->recHeaderCRC = Nu_ReadTwo(pArchive, fp);
    pRecord->recAttribCount = Nu_ReadTwoC(pArchive, fp, &crc);
    pRecord->recVersionNumber = Nu_ReadTwoC(pArchive, fp, &crc);
    pRecord->recTotalThreads = Nu_ReadFourC(pArchive, fp, &crc);
    pRecord->recFileSysID = Nu_ReadTwoC(pArchive, fp, &crc);
    pRecord->recFileSysInfo = Nu_ReadTwoC(pArchive, fp, &crc);
    pRecord->recAccess = Nu_ReadFourC(pArchive, fp, &crc);
    pRecord->recFileType = Nu_ReadFourC(pArchive, fp, &crc);
    pRecord->recExtraType = Nu_ReadFourC(pArchive, fp, &crc);
    pRecord->recStorageType = Nu_ReadTwoC(pArchive, fp, &crc);
    pRecord->recCreateWhen = Nu_ReadDateTimeC(pArchive, fp, &crc);
    pRecord->recModWhen = Nu_ReadDateTimeC(pArchive, fp, &crc);
    pRecord->recArchiveWhen = Nu_ReadDateTimeC(pArchive, fp, &crc);
    bytesRead = 56;     /* 4-byte 'NuFX' plus the above */

    /*
     * Do some sanity checks before we continue.
     */
    if ((err = Nu_HeaderIOFailed(pArchive, fp)) != kNuErrNone) {
        Nu_ReportError(NU_BLOB, err, "Failed reading record header");
        goto bail;
    }
    if (pRecord->recAttribCount > kNuReasonableAttribCount) {
        err = kNuErrBadRecord;
        Nu_ReportError(NU_BLOB, err, "Attrib count is huge (%u)",
            pRecord->recAttribCount);
        goto bail;
    }
    if (pRecord->recVersionNumber > kNuMaxRecordVersion) {
        err = kNuErrBadRecord;
        Nu_ReportError(NU_BLOB, err, "Unrecognized record version number (%u)",
            pRecord->recVersionNumber);
        goto bail;
    }
    if (pRecord->recTotalThreads > kNuReasonableTotalThreads) {
        err = kNuErrBadRecord;
        Nu_ReportError(NU_BLOB, err, "Unreasonable number of threads (%u)",
            pRecord->recTotalThreads);
        goto bail;
    }

    /*
     * Read the option list, if present.
     */
    if (pRecord->recVersionNumber > 0) {
        pRecord->recOptionSize = Nu_ReadTwoC(pArchive, fp, &crc);
        bytesRead += 2;

        /*
         * It appears GS/ShrinkIt is creating bad option lists, claiming
         * 36 bytes of data when there's only room for 18.  Since we don't
         * really pay attention to the option list
         */
        if (pRecord->recOptionSize + bytesRead > pRecord->recAttribCount -2) {
            DBUG(("--- truncating option list from %d to %d\n",
                pRecord->recOptionSize,
                pRecord->recAttribCount -2 - bytesRead));
            if (pRecord->recAttribCount -2 > bytesRead)
                pRecord->recOptionSize = pRecord->recAttribCount -2 - bytesRead;
            else
                pRecord->recOptionSize = 0;
        }

        /* this is the older test, which rejected funky archives */
        if (pRecord->recOptionSize + bytesRead > pRecord->recAttribCount -2) {
            /* option size exceeds the total attribute area */
            err = kNuErrBadRecord;
            Nu_ReportError(NU_BLOB, kNuErrBadRecord,
                "Option size (%u) exceeds attribs (%u,%u-2)",
                    pRecord->recOptionSize, bytesRead,
                    pRecord->recAttribCount);
            goto bail;
        }

        if (pRecord->recOptionSize) {
            pRecord->recOptionList = Nu_Malloc(pArchive,pRecord->recOptionSize);
            BailAlloc(pRecord->recOptionList);
            (void) Nu_ReadBytesC(pArchive, fp, pRecord->recOptionList,
                    pRecord->recOptionSize, &crc);
            bytesRead += pRecord->recOptionSize;
        }
    } else {
        pRecord->recOptionSize = 0;
        pRecord->recOptionList = NULL;
    }

    /* last two bytes are the filename len; all else is "extra" */
    pRecord->extraCount = (pRecord->recAttribCount -2) - bytesRead;
    Assert(pRecord->extraCount >= 0);

    /*
     * Some programs (for example, NuLib) may leave extra junk in here.  This
     * is allowed by the archive spec.  We may want to preserve it, so we
     * allocate space for it and read it if it exists.
     */
    if (pRecord->extraCount) {
        pRecord->extraBytes = Nu_Malloc(pArchive, pRecord->extraCount);
        BailAlloc(pRecord->extraBytes);
        (void) Nu_ReadBytesC(pArchive, fp, pRecord->extraBytes,
                pRecord->extraCount, &crc);
        bytesRead += pRecord->extraCount;
    }

    /*
     * Read the in-record filename if one exists (likely in v0 records only).
     */
    pRecord->recFilenameLength = Nu_ReadTwoC(pArchive, fp, &crc);
    bytesRead += 2;
    if (pRecord->recFilenameLength > kNuReasonableFilenameLen) {
        err = kNuErrBadRecord;
        Nu_ReportError(NU_BLOB, kNuErrBadRecord, "Filename length is huge (%u)",
            pRecord->recFilenameLength);
        goto bail;
    }
    if (pRecord->recFilenameLength) {
        pRecord->recFilenameMOR =
                Nu_Malloc(pArchive, pRecord->recFilenameLength +1);
        BailAlloc(pRecord->recFilenameMOR);
        (void) Nu_ReadBytesC(pArchive, fp, pRecord->recFilenameMOR,
                pRecord->recFilenameLength, &crc);
        pRecord->recFilenameMOR[pRecord->recFilenameLength] = '\0';

        bytesRead += pRecord->recFilenameLength;

        Nu_StripHiIfAllSet(pRecord->recFilenameMOR);
        
        /* use the in-header one */
        pRecord->filenameMOR = pRecord->recFilenameMOR;
    }

    /*
     * Read the threads records.  The data is included in the record header
     * CRC, so we have to pass that in too.
     */
    pRecord->fakeThreads = 0;
    err = Nu_ReadThreadHeaders(pArchive, pRecord, &crc);
    BailError(err);

    /*
     * After all is said and done, did we read the file without errors,
     * and does the CRC match?
     */
    if ((err = Nu_HeaderIOFailed(pArchive, fp)) != kNuErrNone) {
        Nu_ReportError(NU_BLOB, err, "Failed reading late record header");
        goto bail;
    }
    if (!pArchive->valIgnoreCRC && crc != pRecord->recHeaderCRC) {
        if (!Nu_ShouldIgnoreBadCRC(pArchive, pRecord, kNuErrBadRHCRC)) {
            err = kNuErrBadRHCRC;
            Nu_ReportError(NU_BLOB, err, "Stored RH CRC=0x%04x, calc=0x%04x",
                pRecord->recHeaderCRC, crc);
            Nu_ReportError(NU_BLOB_DEBUG, kNuErrNone,
                "--- Problematic record is id=%u", pRecord->recordIdx);
            goto bail;
        }
    }

    /*
     * Init or compute misc record fields.
     */
    /* adjust "currentOffset" for the entire record header */
    pArchive->currentOffset += bytesRead;
    pArchive->currentOffset +=
        (pRecord->recTotalThreads - pRecord->fakeThreads) * kNuThreadHeaderSize;

    pRecord->recHeaderLength =
        bytesRead + pRecord->recTotalThreads * kNuThreadHeaderSize;
    pRecord->recHeaderLength -= pRecord->fakeThreads * kNuThreadHeaderSize;

    err = Nu_ComputeThreadData(pArchive, pRecord);
    BailError(err);

    /* check for "bad Mac" archives */
    if (pArchive->valHandleBadMac) {
        if (pRecord->recFileSysInfo == '?' &&
            pRecord->recFileSysID == kNuFileSysMacMFS)
        {
            DBUG(("--- using 'bad mac' handling\n"));
            pRecord->isBadMac = true;
            pRecord->recFileSysInfo = ':';
        }
    }

bail:
    if (err != kNuErrNone)
        (void)Nu_FreeRecordContents(pArchive, pRecord);
    return err;
}


/*
 * Update the record's storageType if it looks like it needs it, based on
 * the current set of threads.
 *
 * The rules we follow (stopping at the first match) are:
 *  - If there's a disk thread, leave it alone.  Disk block size issues
 *    should already have been resolved.  If we end up copying the same
 *    bogus block size we were given initially, that's fine.
 *  - If there's a resource fork, set the storageType to 5.
 *  - If there's a data fork, set the storageType to 1-3.
 *  - If there are no data-class threads at all, set the storageType to zero.
 *
 * This assumes that all updates have already been processed, i.e. there's
 * no lingering add or delete threadMods.  This only examines the thread
 * array.
 *
 * NOTE: for data files (types 1, 2, and 3), the actual value may not match
 * up what ProDOS would use, because this doesn't test for sparseness.
 */
static void Nu_UpdateStorageType(NuArchive* pArchive, NuRecord* pRecord)
{
    NuError err;
    NuThread* pThread;

    err = Nu_FindThreadByID(pRecord, kNuThreadIDDiskImage, &pThread);
    if (err == kNuErrNone)
        goto bail;

    err = Nu_FindThreadByID(pRecord, kNuThreadIDRsrcFork, &pThread);
    if (err == kNuErrNone) {
        DBUG(("--- setting storageType to %d (was %d)\n", kNuStorageExtended,
            pRecord->recStorageType));
        pRecord->recStorageType = kNuStorageExtended;
        goto bail;
    }

    err = Nu_FindThreadByID(pRecord, kNuThreadIDDataFork, &pThread);
    if (err == kNuErrNone) {
        int newType;
        if (pThread->actualThreadEOF <= 512)
            newType = kNuStorageSeedling;
        else if (pThread->actualThreadEOF < 131072)
            newType = kNuStorageSapling;
        else
            newType = kNuStorageTree;
        DBUG(("--- setting storageType to %d (was %d)\n", newType,
            pRecord->recStorageType));
        pRecord->recStorageType = newType;
        goto bail;
    }

    DBUG(("--- no stuff here, setting storageType to %d (was %d)\n",
        kNuStorageUnknown, pRecord->recStorageType));
    pRecord->recStorageType = kNuStorageUnknown;

bail:
    return;
}

/*
 * Write the record header to the current offset of the specified file.
 * This includes writing all of the thread headers.
 *
 * We don't "promote" records to newer versions, because that might
 * require expanding and CRCing data threads.  Instead, we write the
 * record in a manner appropriate for the version.
 *
 * As a side effect, this may update the storageType to something appropriate.
 *
 * The position of the file pointer on exit is undefined.  The position
 * past the end of the record will be stored in pArchive->currentOffset.
 */
NuError Nu_WriteRecordHeader(NuArchive* pArchive, NuRecord* pRecord, FILE* fp)
{
    NuError err = kNuErrNone;
    uint16_t crc;
    long crcOffset;
    int bytesWritten;

    Assert(pArchive != NULL);
    Assert(pRecord != NULL);
    Assert(fp != NULL);

    /*
     * Before we get started, let's make sure the storageType makes sense
     * for this record.
     */
    Nu_UpdateStorageType(pArchive, pRecord);

    DBUG(("--- Writing record header (v=%d)\n", pRecord->recVersionNumber));
    
    (void) Nu_WriteBytes(pArchive, fp, pRecord->recNufxID, kNufxIDLen);
    err = Nu_FTell(fp, &crcOffset);
    BailError(err);

    /*
     * Write the static fields.
     */
    crc = 0;
    Nu_WriteTwo(pArchive, fp, 0);   /* crc -- come back later */
    Nu_WriteTwoC(pArchive, fp, pRecord->recAttribCount, &crc);
    Nu_WriteTwoC(pArchive, fp, pRecord->recVersionNumber, &crc);
    Nu_WriteFourC(pArchive, fp, pRecord->recTotalThreads, &crc);
    Nu_WriteTwoC(pArchive, fp, (uint16_t)pRecord->recFileSysID, &crc);
    Nu_WriteTwoC(pArchive, fp, pRecord->recFileSysInfo, &crc);
    Nu_WriteFourC(pArchive, fp, pRecord->recAccess, &crc);
    Nu_WriteFourC(pArchive, fp, pRecord->recFileType, &crc);
    Nu_WriteFourC(pArchive, fp, pRecord->recExtraType, &crc);
    Nu_WriteTwoC(pArchive, fp, pRecord->recStorageType, &crc);
    Nu_WriteDateTimeC(pArchive, fp, pRecord->recCreateWhen, &crc);
    Nu_WriteDateTimeC(pArchive, fp, pRecord->recModWhen, &crc);
    Nu_WriteDateTimeC(pArchive, fp, pRecord->recArchiveWhen, &crc);
    bytesWritten = 56;      /* 4-byte 'NuFX' plus the above */

    if ((err = Nu_HeaderIOFailed(pArchive, fp)) != kNuErrNone) {
        Nu_ReportError(NU_BLOB, err, "Failed writing record header");
        goto bail;
    }

    /*
     * Write the option list, if present.
     */
    if (pRecord->recVersionNumber > 0) {
        Nu_WriteTwoC(pArchive, fp, pRecord->recOptionSize, &crc);
        bytesWritten += 2;

        if (pRecord->recOptionSize) {
            Nu_WriteBytesC(pArchive, fp, pRecord->recOptionList,
                pRecord->recOptionSize, &crc);
            bytesWritten += pRecord->recOptionSize;
        }
    }

    /*
     * Preserve whatever miscellaneous junk was left in here by the last guy.
     * We don't know what this is or why it's here, but who knows, maybe
     * it's important.
     *
     * Besides, if we don't, we'll have to go back and fix the attrib count.
     */
    if (pRecord->extraCount) {
        Nu_WriteBytesC(pArchive, fp, pRecord->extraBytes, pRecord->extraCount,
            &crc);
        bytesWritten += pRecord->extraCount;
    }

    /*
     * If the record has a filename in the header, write it, unless
     * recent changes have inspired us to drop the name from the header.
     *
     * Records that begin with no filename will have a default one
     * stuffed in, so it's possible for pRecord->filename to be set
     * already even if there wasn't one in the record. (In such cases,
     * we don't write a name.)
     */
    if (pRecord->recFilenameLength && !pRecord->dropRecFilename) {
        Nu_WriteTwoC(pArchive, fp, pRecord->recFilenameLength, &crc);
        bytesWritten += 2;
        Nu_WriteBytesC(pArchive, fp, pRecord->recFilenameMOR,
            pRecord->recFilenameLength, &crc);
    } else {
        Nu_WriteTwoC(pArchive, fp, 0, &crc);
        bytesWritten += 2;
    }

    /* make sure we are where we thought we would be */
    if (bytesWritten != pRecord->recAttribCount) {
        err = kNuErrInternal;
        Nu_ReportError(NU_BLOB, kNuErrNone,
            "Didn't write what was expected (%d vs %d)",
            bytesWritten, pRecord->recAttribCount);
        goto bail;
    }

    /* write the thread headers, and zero out "fake" thread count */
    err = Nu_WriteThreadHeaders(pArchive, pRecord, fp, &crc);
    BailError(err);

    /* get the current file offset, for some computations later */
    err = Nu_FTell(fp, &pArchive->currentOffset);
    BailError(err);

    /* go back and fill in the CRC */
    pRecord->recHeaderCRC = crc;
    err = Nu_FSeek(fp, crcOffset, SEEK_SET);
    BailError(err);
    Nu_WriteTwo(pArchive, fp, pRecord->recHeaderCRC);

    /*
     * All okay?
     */
    if ((err = Nu_HeaderIOFailed(pArchive, fp)) != kNuErrNone) {
        Nu_ReportError(NU_BLOB, err, "Failed writing late record header");
        goto bail;
    }

    /*
     * Update values for misc record fields.
     */
    Assert(pRecord->fakeThreads == 0);
    pRecord->recHeaderLength =
        bytesWritten + pRecord->recTotalThreads * kNuThreadHeaderSize;
    pRecord->recHeaderLength -= pRecord->fakeThreads * kNuThreadHeaderSize;

    err = Nu_ComputeThreadData(pArchive, pRecord);
    BailError(err);

bail:
    return err;
}


/*
 * Prepare for a "walk" through the records.  This is useful for the
 * "read the TOC as you go" method of archive use.
 */
static NuError Nu_RecordWalkPrepare(NuArchive* pArchive, NuRecord** ppRecord)
{
    NuError err = kNuErrNone;

    Assert(pArchive != NULL);
    Assert(ppRecord != NULL);

    DBUG(("--- walk prep\n"));
    
    *ppRecord = NULL;

    if (!pArchive->haveToc) {
        /* might have tried and aborted earlier, rewind to start of records */
        err = Nu_RewindArchive(pArchive);
        BailError(err);
    }

bail:
    return err;
}

/*
 * Get the next record from the "orig" set in the archive.
 *
 * On entry, pArchive->archiveFp must point at the start of the next
 * record.  On exit, it will point past the end of the record (headers and
 * all data) that we just read.
 *
 * If we have the TOC, we just pull it out of the structure.  If we don't,
 * we read it from the archive file, and add it to the TOC being
 * constructed.
 */
static NuError Nu_RecordWalkGetNext(NuArchive* pArchive, NuRecord** ppRecord)
{
    NuError err = kNuErrNone;

    Assert(pArchive != NULL);
    Assert(ppRecord != NULL);

    /*DBUG(("--- walk toc=%d\n", pArchive->haveToc));*/

    if (pArchive->haveToc) {
        if (*ppRecord == NULL)
            *ppRecord = Nu_RecordSet_GetListHead(&pArchive->origRecordSet);
        else
            *ppRecord = (*ppRecord)->pNext;
    } else {
        *ppRecord = NULL;    /* so we don't try to free it on exit */

        /* allocate and fill in a new record */
        err = Nu_RecordNew(pArchive, ppRecord);
        BailError(err);

        /* read data from archive file */
        err = Nu_ReadRecordHeader(pArchive, *ppRecord);
        BailError(err);
        err = Nu_ScanThreads(pArchive, *ppRecord, (*ppRecord)->recTotalThreads);
        BailError(err);

        DBUG(("--- Found record '%s'\n", (*ppRecord)->filenameMOR));

        /* add to list */
        err = Nu_RecordSet_AddRecord(&pArchive->origRecordSet, *ppRecord);
        BailError(err);
    }

bail:
    if (err != kNuErrNone && !pArchive->haveToc) {
        /* on failure, free whatever we allocated */
        Nu_RecordFree(pArchive, *ppRecord);
        *ppRecord = NULL;
    }
    return err;
}

/*
 * Finish off a successful record walk by noting that we now have a
 * full table of contents.  On an unsuccessful walk, blow away the TOC
 * if we don't have all of it.
 */
static NuError Nu_RecordWalkFinish(NuArchive* pArchive, NuError walkErr)
{
    if (pArchive->haveToc)
        return kNuErrNone;

    if (walkErr == kNuErrNone) {
        pArchive->haveToc = true;
        /* mark as loaded, even if there weren't any entries (e.g. new arc) */
        Nu_RecordSet_SetLoaded(&pArchive->origRecordSet, true);
        return kNuErrNone;
    } else {
        pArchive->haveToc = false;  /* redundant */
        return Nu_RecordSet_FreeAllRecords(pArchive, &pArchive->origRecordSet);
    }
}


/*
 * If we don't have the complete record listing from the archive in
 * the "orig" record set, go get it.
 *
 * Uses the "record walk" functions, because they're there.
 */
NuError Nu_GetTOCIfNeeded(NuArchive* pArchive)
{
    NuError err = kNuErrNone;
    NuRecord* pRecord;
    uint32_t count;

    Assert(pArchive != NULL);

    if (pArchive->haveToc)
        goto bail;

    DBUG(("--- GetTOCIfNeeded\n"));

    err = Nu_RecordWalkPrepare(pArchive, &pRecord);
    BailError(err);

    count = pArchive->masterHeader.mhTotalRecords;
    while (count--) {
        err = Nu_RecordWalkGetNext(pArchive, &pRecord);
        BailError(err);
    }

bail:
    (void) Nu_RecordWalkFinish(pArchive, err);
    return err;
}



/*
 * ===========================================================================
 *      Streaming read-only operations
 * ===========================================================================
 */

/*
 * Run through the entire archive, pulling out the header bits, skipping
 * over the data bits, and calling "contentFunc" for each record.
 */
NuError Nu_StreamContents(NuArchive* pArchive, NuCallback contentFunc)
{
    NuError err = kNuErrNone;
    NuRecord tmpRecord;
    NuResult result;
    uint32_t count;

    if (contentFunc == NULL) {
        err = kNuErrInvalidArg;
        goto bail;
    }

    Nu_InitRecordContents(pArchive, &tmpRecord);
    count = pArchive->masterHeader.mhTotalRecords;

    while (count--) {
        err = Nu_ReadRecordHeader(pArchive, &tmpRecord);
        BailError(err);
        err = Nu_ScanThreads(pArchive, &tmpRecord, tmpRecord.recTotalThreads);
        BailError(err);

        /*Nu_DebugDumpRecord(&tmpRecord);
        printf("\n");*/

        /* let them display the contents */
        result = (*contentFunc)(pArchive, &tmpRecord);
        if (result == kNuAbort) {
            err = kNuErrAborted;
            goto bail;
        }

        /* dispose of the entry */
        (void) Nu_FreeRecordContents(pArchive, &tmpRecord);
        (void) Nu_InitRecordContents(pArchive, &tmpRecord);
    }

bail:
    (void) Nu_FreeRecordContents(pArchive, &tmpRecord);
    return err;
}


/*
 * If we're trying to be compatible with ShrinkIt, and we tried to extract
 * a record that had nothing in it but comments and filenames, then we need
 * to create a zero-byte data file.
 *
 * GS/ShrinkIt v1.1 has a bug that causes it to store zero-byte data files
 * (and, for that matter, zero-byte resource forks) without a thread header.
 * It isn't able to extract them.  This isn't so much a compatibility
 * thing as it is a bug-workaround thing.
 *
 * The record's storage type should tell us if it was an extended file or
 * a plain file.  Not really important when extracting, but if we want
 * to recreate the original we need to re-add the resource fork so
 * NufxLib knows to make it an extended file.
 */
static NuError Nu_FakeZeroExtract(NuArchive* pArchive, NuRecord* pRecord,
    int threadKind)
{
    NuError err;
    NuThread fakeThread;

    Assert(pRecord != NULL);

    DBUG(("--- found empty record, creating zero-byte file (kind=0x%04x)\n",
        threadKind));
    fakeThread.thThreadClass = kNuThreadClassData;
    fakeThread.thThreadFormat = kNuThreadFormatUncompressed;
    fakeThread.thThreadKind = threadKind;
    fakeThread.thThreadCRC = kNuInitialThreadCRC;
    fakeThread.thThreadEOF = 0;
    fakeThread.thCompThreadEOF = 0;

    fakeThread.threadIdx = (NuThreadIdx)-1; /* shouldn't matter */
    fakeThread.actualThreadEOF = 0;
    fakeThread.fileOffset = 0;                  /* shouldn't matter */
    fakeThread.used = false;

    err = Nu_ExtractThreadBulk(pArchive, pRecord, &fakeThread);
    if (err == kNuErrSkipped)
        err = Nu_SkipThread(pArchive, pRecord, &fakeThread);

    return err;
}


/*
 * Run through the entire archive, extracting the contents.
 */
NuError Nu_StreamExtract(NuArchive* pArchive)
{
    NuError err = kNuErrNone;
    NuRecord tmpRecord;
    Boolean needFakeData, needFakeRsrc;
    uint32_t count;
    long idx;

    /* reset this just to be safe */
    pArchive->lastDirCreatedUNI = NULL;

    Nu_InitRecordContents(pArchive, &tmpRecord);
    count = pArchive->masterHeader.mhTotalRecords;

    while (count--) {
        /*
         * Read the record header (which includes the thread header blocks).
         */
        err = Nu_ReadRecordHeader(pArchive, &tmpRecord);
        BailError(err);

        /*
         * We may need to pull the filename out of a thread, but we don't
         * want to blow past any data while we do it.  There's no really
         * good way to deal with this, so we just assume that all NuFX
         * applications are nice and put the filename thread first.
         */
        for (idx = 0; idx < (long)tmpRecord.recTotalThreads; idx++) {
            const NuThread* pThread = Nu_GetThread(&tmpRecord, idx);

            if (NuMakeThreadID(pThread->thThreadClass, pThread->thThreadKind)
                == kNuThreadIDFilename)
            {
                break;
            }
        }
        /* if we have fn, read it; either way, leave idx pointing at next */
        if (idx < (long)tmpRecord.recTotalThreads) {
            idx++;      /* want count, not index */
            err = Nu_ScanThreads(pArchive, &tmpRecord, idx);
            BailError(err);
        } else
            idx = 0;
        if (tmpRecord.filenameMOR == NULL) {
            Nu_ReportError(NU_BLOB, kNuErrNone,
                "Couldn't find filename in record");
            err = kNuErrBadRecord;
            goto bail;
        }

        /*Nu_DebugDumpRecord(&tmpRecord);
        printf("\n");*/

        needFakeData = true;
        needFakeRsrc = (tmpRecord.recStorageType == kNuStorageExtended);

        /* extract all relevant (remaining) threads */
        pArchive->lastFileCreatedUNI = NULL;
        for ( ; idx < (long)tmpRecord.recTotalThreads; idx++) {
            const NuThread* pThread = Nu_GetThread(&tmpRecord, idx);

            if (pThread->thThreadClass == kNuThreadClassData) {
                if (pThread->thThreadKind == kNuThreadKindDataFork) {
                    needFakeData = false;
                } else if (pThread->thThreadKind == kNuThreadKindRsrcFork) {
                    needFakeRsrc = false;
                } else if (pThread->thThreadKind == kNuThreadKindDiskImage) {
                    /* needFakeRsrc shouldn't be set, but clear anyway */
                    needFakeData = needFakeRsrc = false;
                }
                err = Nu_ExtractThreadBulk(pArchive, &tmpRecord, pThread);
                if (err == kNuErrSkipped) {
                    err = Nu_SkipThread(pArchive, &tmpRecord, pThread);
                    BailError(err);
                } else if (err != kNuErrNone)
                    goto bail;
            } else {
                DBUG(("IGNORING 0x%08lx from '%s'\n",
                  NuMakeThreadID(pThread->thThreadClass, pThread->thThreadKind),
                    tmpRecord.filename));
                if (NuGetThreadID(pThread) != kNuThreadIDComment &&
                    NuGetThreadID(pThread) != kNuThreadIDFilename)
                {
                    /* unknown stuff in record, skip thread fakery */
                    needFakeData = needFakeRsrc = false;
                }
                err = Nu_SkipThread(pArchive, &tmpRecord, pThread);
                BailError(err);
            }
        }

        /*
         * As in Nu_ExtractRecordByPtr, we need to synthesize empty forks for
         * cases where GSHK omitted the data thread entirely.
         */
        Assert(!pArchive->valMaskDataless || (!needFakeData && !needFakeRsrc));
        if (needFakeData) {
            err = Nu_FakeZeroExtract(pArchive, &tmpRecord,
                    kNuThreadKindDataFork);
            BailError(err);
        }
        if (needFakeRsrc) {
            err = Nu_FakeZeroExtract(pArchive, &tmpRecord,
                    kNuThreadKindRsrcFork);
            BailError(err);
        }

        /* dispose of the entry */
        (void) Nu_FreeRecordContents(pArchive, &tmpRecord);
        (void) Nu_InitRecordContents(pArchive, &tmpRecord);
    }

bail:
    (void) Nu_FreeRecordContents(pArchive, &tmpRecord);
    return err;
}

/*
 * Test the contents of an archive.  Works just like extraction, but we
 * don't store anything.
 */
NuError Nu_StreamTest(NuArchive* pArchive)
{
    NuError err;

    pArchive->testMode = true;
    err = Nu_StreamExtract(pArchive);
    pArchive->testMode = false;
    return err;
}


/*
 * ===========================================================================
 *      Non-streaming read-only operations
 * ===========================================================================
 */

/*
 * Shove the archive table of contents through the callback function.
 *
 * This only walks through the "orig" list, so it does not reflect the
 * results of un-flushed changes.
 */
NuError Nu_Contents(NuArchive* pArchive, NuCallback contentFunc)
{
    NuError err = kNuErrNone;
    NuRecord* pRecord;
    NuResult result;
    uint32_t count;

    if (contentFunc == NULL) {
        err = kNuErrInvalidArg;
        goto bail;
    }

    err = Nu_RecordWalkPrepare(pArchive, &pRecord);
    BailError(err);

    count = pArchive->masterHeader.mhTotalRecords;
    while (count--) {
        err = Nu_RecordWalkGetNext(pArchive, &pRecord);
        BailError(err);

        Assert(pRecord->filenameMOR != NULL);
        result = (*contentFunc)(pArchive, pRecord);
        if (result == kNuAbort) {
            err = kNuErrAborted;
            goto bail;
        }
    }

bail:
    (void) Nu_RecordWalkFinish(pArchive, err);
    return err;
}


/*
 * Extract all interesting threads from a record, given a NuRecord pointer
 * into the archive data structure.
 *
 * This assumes random access, so it can't be used in streaming mode.
 */
static NuError Nu_ExtractRecordByPtr(NuArchive* pArchive, NuRecord* pRecord)
{
    NuError err = kNuErrNone;
    Boolean needFakeData, needFakeRsrc;
    uint32_t idx;

    needFakeData = true;
    needFakeRsrc = (pRecord->recStorageType == kNuStorageExtended);

    Assert(!Nu_IsStreaming(pArchive));  /* we don't skip things we don't read */
    Assert(pRecord != NULL);

    /* extract all relevant threads */
    pArchive->lastFileCreatedUNI = NULL;
    for (idx = 0; idx < pRecord->recTotalThreads; idx++) {
        const NuThread* pThread = Nu_GetThread(pRecord, idx);

        if (pThread->thThreadClass == kNuThreadClassData) {
            if (pThread->thThreadKind == kNuThreadKindDataFork) {
                needFakeData = false;
            } else if (pThread->thThreadKind == kNuThreadKindRsrcFork) {
                needFakeRsrc = false;
            } else if (pThread->thThreadKind == kNuThreadKindDiskImage) {
                /* needFakeRsrc shouldn't be set, but clear anyway */
                needFakeData = needFakeRsrc = false;
            }
            err = Nu_ExtractThreadBulk(pArchive, pRecord, pThread);
            if (err == kNuErrSkipped) {
                err = Nu_SkipThread(pArchive, pRecord, pThread);
                BailError(err);
            } else if (err != kNuErrNone)
                goto bail;
        } else {
            if (NuGetThreadID(pThread) != kNuThreadIDComment &&
                NuGetThreadID(pThread) != kNuThreadIDFilename)
            {
                /*
                 * This record has a thread we don't recognize.  Disable
                 * the thread fakery to avoid doing anything weird -- we
                 * should only need to create zero-length files for
                 * simple file records.
                 */
                needFakeData = needFakeRsrc = false;
            }
            DBUG(("IGNORING 0x%08lx from '%s'\n",
                NuMakeThreadID(pThread->thThreadClass, pThread->thThreadKind),
                pRecord->filenameMOR));
        }
    }

    /*
     * GSHK creates empty threads for zero-length forks.  It doesn't always
     * handle them correctly when extracting, so it appears this behavior
     * may not be intentional.
     *
     * We need to create an empty file for whichever forks are missing.
     * Could be the data fork, resource fork, or both.  The only way to
     * know what's expected is to examine the file's storage type.
     *
     * If valMaskDataless is enabled, this won't fire, because we will have
     * "forged" the appropriate threads.
     *
     * Note there's another one of these below, in Nu_StreamExtract.
     */
    Assert(!pArchive->valMaskDataless || (!needFakeData && !needFakeRsrc));
    if (needFakeData) {
        err = Nu_FakeZeroExtract(pArchive, pRecord, kNuThreadKindDataFork);
        BailError(err);
    }
    if (needFakeRsrc) {
        err = Nu_FakeZeroExtract(pArchive, pRecord, kNuThreadKindRsrcFork);
        BailError(err);
    }

bail:
    return err;
}


/*
 * Extract a big buncha files.
 */
NuError Nu_Extract(NuArchive* pArchive)
{
    NuError err;
    NuRecord* pRecord = NULL;
    uint32_t count;
    long offset;

    /* reset this just to be safe */
    pArchive->lastDirCreatedUNI = NULL;

    err = Nu_RecordWalkPrepare(pArchive, &pRecord);
    BailError(err);

    count = pArchive->masterHeader.mhTotalRecords;
    while (count--) {
        /* read the record and threads if we don't have them yet */
        err = Nu_RecordWalkGetNext(pArchive, &pRecord);
        BailError(err);

        if (!pArchive->haveToc) {
            /* remember where the end of the record is */
            err = Nu_FTell(pArchive->archiveFp, &offset);
            BailError(err);
        }

        /* extract one or more threads */
        err = Nu_ExtractRecordByPtr(pArchive, pRecord);
        BailError(err);

        if (!pArchive->haveToc) {
            /* line us back up so RecordWalkGetNext can read the record hdr */
            err = Nu_FSeek(pArchive->archiveFp, offset, SEEK_SET);
            BailError(err);
        }
    }

bail:
    (void) Nu_RecordWalkFinish(pArchive, err);
    return err;
}


/*
 * Extract a single record.
 */
NuError Nu_ExtractRecord(NuArchive* pArchive, NuRecordIdx recIdx)
{
    NuError err;
    NuRecord* pRecord;

    if (Nu_IsStreaming(pArchive))
        return kNuErrUsage;
    err = Nu_GetTOCIfNeeded(pArchive);
    BailError(err);

    /* find the correct record by index */
    err = Nu_RecordSet_FindByIdx(&pArchive->origRecordSet, recIdx, &pRecord);
    BailError(err);
    Assert(pRecord != NULL);

    /* extract whatever looks promising */
    err = Nu_ExtractRecordByPtr(pArchive, pRecord);
    BailError(err);

bail:
    return err;
}


/*
 * Test the contents of an archive.  Works just like extraction, but we
 * don't store anything.
 */
NuError Nu_Test(NuArchive* pArchive)
{
    NuError err;

    pArchive->testMode = true;
    err = Nu_Extract(pArchive);
    pArchive->testMode = false;
    return err;
}

/*
 * Test a single record.
 */
NuError Nu_TestRecord(NuArchive* pArchive, NuRecordIdx recIdx)
{
    NuError err;
    NuRecord* pRecord;

    if (Nu_IsStreaming(pArchive))
        return kNuErrUsage;
    err = Nu_GetTOCIfNeeded(pArchive);
    BailError(err);

    /* find the correct record by index */
    err = Nu_RecordSet_FindByIdx(&pArchive->origRecordSet, recIdx, &pRecord);
    BailError(err);
    Assert(pRecord != NULL);

    /* extract whatever looks promising */
    pArchive->testMode = true;
    err = Nu_ExtractRecordByPtr(pArchive, pRecord);
    pArchive->testMode = false;
    BailError(err);

bail:
    return err;
}


/*
 * Return a pointer to a NuRecord.
 *
 * This pulls the record out of the "orig" set, so it will work even
 * for records that have been deleted.  It will not reflect changes
 * made by previous "write" calls, not even SetRecordAttr.
 */
NuError Nu_GetRecord(NuArchive* pArchive, NuRecordIdx recordIdx,
    const NuRecord** ppRecord)
{
    NuError err;

    if (recordIdx == 0 || ppRecord == NULL)
        return kNuErrInvalidArg;

    if (Nu_IsStreaming(pArchive))
        return kNuErrUsage;
    err = Nu_GetTOCIfNeeded(pArchive);
    BailError(err);

    err = Nu_RecordSet_FindByIdx(&pArchive->origRecordSet, recordIdx,
            (NuRecord**)ppRecord);
    if (err == kNuErrNone) {
        Assert(*ppRecord != NULL);
    }
    /* fall through with error */

bail:
    return err;
}

/*
 * Find the recordIdx of a record by storage name.
 */
NuError Nu_GetRecordIdxByName(NuArchive* pArchive, const char* nameMOR,
    NuRecordIdx* pRecordIdx)
{
    NuError err;
    NuRecord* pRecord = NULL;

    if (pRecordIdx == NULL)
        return kNuErrInvalidArg;

    if (Nu_IsStreaming(pArchive))
        return kNuErrUsage;
    err = Nu_GetTOCIfNeeded(pArchive);
    BailError(err);

    err = Nu_RecordSet_FindByName(&pArchive->origRecordSet, nameMOR, &pRecord);
    if (err == kNuErrNone) {
        Assert(pRecord != NULL);
        *pRecordIdx = pRecord->recordIdx;
    }
    /* fall through with error */

bail:
    return err;
}

/*
 * Find the recordIdx of a record by zero-based position.
 */
NuError Nu_GetRecordIdxByPosition(NuArchive* pArchive, uint32_t position,
    NuRecordIdx* pRecordIdx)
{
    NuError err;
    const NuRecord* pRecord;

    if (pRecordIdx == NULL)
        return kNuErrInvalidArg;

    if (Nu_IsStreaming(pArchive))
        return kNuErrUsage;
    err = Nu_GetTOCIfNeeded(pArchive);
    BailError(err);

    if (position >= Nu_RecordSet_GetNumRecords(&pArchive->origRecordSet)) {
        err = kNuErrRecordNotFound;
        goto bail;
    }

    pRecord = Nu_RecordSet_GetListHead(&pArchive->origRecordSet);
    while (position--) {
        Assert(pRecord->pNext != NULL);
        pRecord = pRecord->pNext;
    }

    *pRecordIdx = pRecord->recordIdx;

bail:
    return err;
}


/*
 * ===========================================================================
 *      Read/write record operations (add, delete)
 * ===========================================================================
 */

/*
 * Find an existing record somewhere in the archive.  If the "copy" set
 * exists it will be searched.  If not, the "orig" set is searched, and
 * if an entry is found a "copy" set will be created.
 *
 * The goal is to always return something from the "copy" set, which we
 * could do easily by just creating the "copy" set and then searching in
 * it.  However, we don't want to create the "copy" set if we don't have
 * to, so we search "orig" if "copy" doesn't exist yet.
 *
 * The record returned will always be from the "copy" set.  An error result
 * is returned if the record isn't found.
 */
NuError Nu_FindRecordForWriteByIdx(NuArchive* pArchive, NuRecordIdx recIdx,
    NuRecord** ppFoundRecord)
{
    NuError err;

    Assert(pArchive != NULL);
    Assert(ppFoundRecord != NULL);

    if (Nu_RecordSet_GetLoaded(&pArchive->copyRecordSet)) {
        err = Nu_RecordSet_FindByIdx(&pArchive->copyRecordSet, recIdx,
                ppFoundRecord);
    } else {
        Assert(Nu_RecordSet_GetLoaded(&pArchive->origRecordSet));
        err = Nu_RecordSet_FindByIdx(&pArchive->origRecordSet, recIdx,
                ppFoundRecord);
        *ppFoundRecord = NULL;       /* can't delete from here */
    }
    BailErrorQuiet(err);

    /*
     * The record exists.  If we were looking in the "orig" set, we have
     * to create a "copy" set and return it from there.
     */
    if (*ppFoundRecord == NULL) {
        err = Nu_RecordSet_Clone(pArchive, &pArchive->copyRecordSet,
                &pArchive->origRecordSet);
        BailError(err);
        err = Nu_RecordSet_FindByIdx(&pArchive->copyRecordSet, recIdx,
                ppFoundRecord);
        Assert(err == kNuErrNone && *ppFoundRecord != NULL); /* must succeed */
        BailError(err);
    }

bail:
    return err;
}


/*
 * Deal with the situation where we're trying to add a record with the
 * same name as an existing record.  The existing record can't be in the
 * "new" list (that's handled differently) and can't already have been
 * deleted.
 *
 * This will either delete the existing record or return with an error.
 *
 * If we decide to delete the record, and the "orig" record set was
 * passed in, then the record will be deleted from the "copy" set (which
 * will be created only if necessary).
 */
static NuError Nu_HandleAddDuplicateRecord(NuArchive* pArchive,
    NuRecordSet* pRecordSet, NuRecord* pRecord,
    const NuFileDetails* pFileDetails)
{
    NuError err = kNuErrNone;
    NuErrorStatus errorStatus;
    NuResult result;

    Assert(pRecordSet == &pArchive->origRecordSet ||
           pRecordSet == &pArchive->copyRecordSet);
    Assert(pRecord != NULL);
    Assert(pFileDetails != NULL);
    Assert(pArchive->valAllowDuplicates == false);

    /*
     * If "only update older" is set, check the dates.  Reject the
     * request if the archived file isn't older than the new file.  This
     * tells the application that the request was rejected, but it's
     * okay for them to move on to the next file.
     */
    if (pArchive->valOnlyUpdateOlder) {
        if (!Nu_IsOlder(&pRecord->recModWhen, &pFileDetails->modWhen))
            return kNuErrNotNewer;
    }

    /*
     * The file exists when it shouldn't.  Decide what to do, based
     * on the options configured by the application.
     *
     * If they "might" allow overwrites, and they have an error-handling
     * callback defined, call that to find out what they want to do
     * here.  Options include skipping or overwriting the record.
     *
     * We don't currently allow renaming of records, though I suppose we
     * could.
     */
    switch (pArchive->valHandleExisting) {
    case kNuMaybeOverwrite:
        if (pArchive->errorHandlerFunc != NULL) {
            errorStatus.operation = kNuOpAdd;
            errorStatus.err = kNuErrRecordExists;
            errorStatus.sysErr = 0;
            errorStatus.message = NULL;
            errorStatus.pRecord = pRecord;
            UNICHAR* pathnameUNI =
                    Nu_CopyMORToUNI(pFileDetails->storageNameMOR);
            errorStatus.pathnameUNI = pathnameUNI;
            errorStatus.origPathname = pFileDetails->origName;
            errorStatus.filenameSeparator =
                                NuGetSepFromSysInfo(pFileDetails->fileSysInfo);
            /*errorStatus.origArchiveTouched = false;*/
            errorStatus.canAbort = true;
            errorStatus.canRetry = false;
            errorStatus.canIgnore = false;
            errorStatus.canSkip = true;
            errorStatus.canRename = false;
            errorStatus.canOverwrite = true;

            result = (*pArchive->errorHandlerFunc)(pArchive, &errorStatus);
            Nu_Free(pArchive, pathnameUNI);

            switch (result) {
            case kNuAbort:
                err = kNuErrAborted;
                goto bail;
            case kNuSkip:
                err = kNuErrSkipped;
                goto bail;
            case kNuOverwrite:
                break;  /* fall back into main code */
            case kNuRetry:
            case kNuRename:
            case kNuIgnore:
            default:
                err = kNuErrSyntax;
                Nu_ReportError(NU_BLOB, err,
                    "Wasn't expecting result %d here", result);
                goto bail;
            }
        } else {
            /* no error handler, treat like NeverOverwrite */
            err = kNuErrSkipped;
            goto bail;
        }
        break;
    case kNuNeverOverwrite:
        err = kNuErrSkipped;
        goto bail;
    case kNuMustOverwrite:
    case kNuAlwaysOverwrite:
        /* fall through to record deletion */
        break;
    default:
        Assert(0);
        err = kNuErrInternal;
        goto bail;
    }

    err = kNuErrNone;

    /*
     * We're going to overwrite the existing record.  To do this, we have
     * to start by deleting it from the "copy" list.
     *
     * If the copy set doesn't yet exist, we have to create it and find
     * the record in the new set.
     */
    if (pRecordSet == &pArchive->origRecordSet) {
        Assert(!Nu_RecordSet_GetLoaded(&pArchive->copyRecordSet));
        err = Nu_RecordSet_Clone(pArchive, &pArchive->copyRecordSet,
                &pArchive->origRecordSet);
        BailError(err);

        err = Nu_RecordSet_FindByIdx(&pArchive->copyRecordSet,
                pRecord->recordIdx, &pRecord);
        Assert(err == kNuErrNone && pRecord != NULL);    /* must succeed */
        BailError(err);
    }

    DBUG(("+++ deleting record %ld\n", pRecord->recordIdx));
    err = Nu_RecordSet_DeleteRecord(pArchive,&pArchive->copyRecordSet, pRecord);
    BailError(err);

bail:
    return err;
}

/*
 * Create a new record, filling in most of the blanks from "pFileDetails".
 *
 * The filename in pFileDetails->storageName will be remembered.  If no
 * filename thread is added to this record before the next Flush call, a
 * filename thread will be generated from this name.
 *
 * This always creates a "version 3" record, regardless of what else is
 * in the archive.  The filename is always in a thread.
 *
 * On success, the NuRecordIdx of the newly-created record will be placed
 * in "*pRecordIdx", and the NuThreadIdx of the filename thread will be
 * placed in "*pThreadIdx".  If "*ppNewRecord" is non-NULL, it gets a pointer
 * to the newly-created record (this isn't part of the external interface).
 */
NuError Nu_AddRecord(NuArchive* pArchive, const NuFileDetails* pFileDetails,
    NuRecordIdx* pRecordIdx, NuRecord** ppNewRecord)
{
    NuError err;
    NuRecord* pNewRecord = NULL;

    if (pFileDetails == NULL || pFileDetails->storageNameMOR == NULL ||
        pFileDetails->storageNameMOR[0] == '\0' ||
        NuGetSepFromSysInfo(pFileDetails->fileSysInfo) == 0)
        /* pRecordIdx may be NULL */
        /* ppNewRecord may be NULL */
    {
        err = kNuErrInvalidArg;
        goto bail;
    }

    if (Nu_IsReadOnly(pArchive))
        return kNuErrArchiveRO;
    err = Nu_GetTOCIfNeeded(pArchive);
    BailError(err);

    /* NuFX spec forbids leading fssep chars */
    if (pFileDetails->storageNameMOR[0] ==
        NuGetSepFromSysInfo(pFileDetails->fileSysInfo))
    {
        err = kNuErrLeadingFssep;
        goto bail;
    }

    /*
     * If requested, look for an existing record.  Look in the "copy"
     * list if we have it (so we don't complain if they've already deleted
     * the record), or in the "orig" list if we don't.  Look in the "new"
     * list to see if it clashes with something we've just added.
     *
     * If this is a brand-new archive, there won't be an "orig" list
     * either.
     */
    if (!pArchive->valAllowDuplicates) {
        NuRecordSet* pRecordSet;
        NuRecord* pFoundRecord;

        pRecordSet = &pArchive->copyRecordSet;
        if (!Nu_RecordSet_GetLoaded(pRecordSet))
            pRecordSet = &pArchive->origRecordSet;
        Assert(Nu_RecordSet_GetLoaded(pRecordSet));
        err = Nu_RecordSet_FindByName(pRecordSet, pFileDetails->storageNameMOR,
                &pFoundRecord);
        if (err == kNuErrNone) {
            /* handle the existing record */
            DBUG(("--- Duplicate record found (%06ld) '%s'\n",
                pFoundRecord->recordIdx, pFoundRecord->filenameMOR));
            err = Nu_HandleAddDuplicateRecord(pArchive, pRecordSet,
                    pFoundRecord, pFileDetails);
            if (err != kNuErrNone) {
                /* for whatever reason, we're not replacing it */
                DBUG(("--- Returning err=%d\n", err));
                goto bail;
            }
        } else {
            /* if we *must* replace an existing file, we fail now */
            if (pArchive->valHandleExisting == kNuMustOverwrite) {
                DBUG(("+++ can't freshen nonexistent '%s'\n",
                    pFileDetails->storageName));
                err = kNuErrDuplicateNotFound;
                goto bail;
            }
        }

        if (Nu_RecordSet_GetLoaded(&pArchive->newRecordSet)) {
            err = Nu_RecordSet_FindByName(&pArchive->newRecordSet,
                    pFileDetails->storageNameMOR, &pFoundRecord);
            if (err == kNuErrNone) {
                /* we can't delete from the "new" list, so return an error */
                err = kNuErrRecordExists;
                goto bail;
            }
        }

        /* clear "err" so we can continue */
        err = kNuErrNone;
    }

    /*
     * Prepare the new record structure.
     */
    err = Nu_RecordNew(pArchive, &pNewRecord);
    BailError(err);
    (void) Nu_InitRecordContents(pArchive, pNewRecord);
    memcpy(pNewRecord->recNufxID, kNufxID, kNufxIDLen);
    /*pNewRecord->recHeaderCRC*/
    /*pNewRecord->recAttribCount*/
    pNewRecord->recVersionNumber = kNuOurRecordVersion;
    pNewRecord->recTotalThreads = 0;
    pNewRecord->recFileSysID = pFileDetails->fileSysID;
    pNewRecord->recFileSysInfo = pFileDetails->fileSysInfo;
    pNewRecord->recAccess = pFileDetails->access;
    pNewRecord->recFileType = pFileDetails->fileType;
    pNewRecord->recExtraType = pFileDetails->extraType;
    pNewRecord->recStorageType = pFileDetails->storageType;
    pNewRecord->recCreateWhen = pFileDetails->createWhen;
    pNewRecord->recModWhen = pFileDetails->modWhen;
    pNewRecord->recArchiveWhen = pFileDetails->archiveWhen;
    pNewRecord->recOptionSize = 0;
    pNewRecord->extraCount = 0;
    pNewRecord->recFilenameLength = 0;

    pNewRecord->recordIdx = Nu_GetNextRecordIdx(pArchive);
    pNewRecord->threadFilenameMOR = NULL;
    pNewRecord->newFilenameMOR = strdup(pFileDetails->storageNameMOR);
    pNewRecord->filenameMOR = pNewRecord->newFilenameMOR;
    pNewRecord->recHeaderLength = -1;
    pNewRecord->totalCompLength = 0;
    pNewRecord->fakeThreads = 0;
    pNewRecord->fileOffset = -1;

    /*
     * Add it to the "new" record set.
     */
    err = Nu_RecordSet_AddRecord(&pArchive->newRecordSet, pNewRecord);
    BailError(err);

    /* return values */
    if (pRecordIdx != NULL)
        *pRecordIdx = pNewRecord->recordIdx;
    if (ppNewRecord != NULL)
        *ppNewRecord = pNewRecord;

bail:
    return err;
}


/*
 * Add a new "add file" thread mod to the specified record.
 *
 * The caller should have already verified that there isn't another
 * "add file" thread mod with the same ThreadID.
 */
static NuError Nu_AddFileThreadMod(NuArchive* pArchive, NuRecord* pRecord,
    const UNICHAR* pathnameUNI, const NuFileDetails* pFileDetails,
    Boolean fromRsrcFork)
{
    NuError err;
    NuThreadFormat threadFormat;
    NuDataSource* pDataSource = NULL;
    NuThreadMod* pThreadMod = NULL;

    Assert(pArchive != NULL);
    Assert(pRecord != NULL);
    Assert(pathnameUNI != NULL);
    Assert(pFileDetails != NULL);
    Assert(fromRsrcFork == true || fromRsrcFork == false);

    if (Nu_IsReadOnly(pArchive))
        return kNuErrArchiveRO;

    /* decide if this should be compressed; we know source isn't */
    if (Nu_IsCompressibleThreadID(pFileDetails->threadID))
        threadFormat = Nu_ConvertCompressValToFormat(pArchive,
                            pArchive->valDataCompression);
    else
        threadFormat = kNuThreadFormatUncompressed;

    /* create a data source for this file, which is assumed uncompressed */
    err = Nu_DataSourceFile_New(kNuThreadFormatUncompressed, 0,
            pathnameUNI, fromRsrcFork, &pDataSource);
    BailError(err);

    /* create a new ThreadMod */
    err = Nu_ThreadModAdd_New(pArchive, pFileDetails->threadID, threadFormat,
            pDataSource, &pThreadMod);
    BailError(err);
    Assert(pThreadMod != NULL);
    /*pDataSource = NULL;*/  /* ThreadModAdd_New makes a copy */

    /* add the thread mod to the record */
    Nu_RecordAddThreadMod(pRecord, pThreadMod);
    pThreadMod = NULL;   /* don't free on exit */

bail:
    if (pDataSource != NULL)
        Nu_DataSourceFree(pDataSource);
    if (pThreadMod != NULL)
        Nu_ThreadModFree(pArchive, pThreadMod);
    return err;
}

/*
 * Make note of a file to add.  This goes beyond AddRecord and AddThread
 * calls by searching the list of newly-added files for matching pairs
 * of data and rsrc forks.  This is independent of the "overwrite existing
 * files" feature.  The comparison is made based on storageName.
 *
 * "fromRsrcFork" tells us how to open the source file, not what type
 * of thread the file should be stored as.
 *
 * If "pRecordIdx" is non-NULL, it will receive the newly assigned recordID.
 */
NuError Nu_AddFile(NuArchive* pArchive, const UNICHAR* pathnameUNI,
    const NuFileDetails* pFileDetails, Boolean fromRsrcFork,
    NuRecordIdx* pRecordIdx)
{
    NuError err = kNuErrNone;
    NuRecordIdx recordIdx = 0;
    NuRecord* pRecord;

    if (pathnameUNI == NULL || pFileDetails == NULL ||
        !(fromRsrcFork == true || fromRsrcFork == false))
    {
        return kNuErrInvalidArg;
    }

    if (Nu_IsReadOnly(pArchive))
        return kNuErrArchiveRO;
    err = Nu_GetTOCIfNeeded(pArchive);
    BailError(err);

    if (pFileDetails->storageNameMOR == NULL) {
        err = kNuErrInvalidArg;
        Nu_ReportError(NU_BLOB, err, "Must specify storageName");
        goto bail;
    }
    if (pFileDetails->storageNameMOR[0] ==
        NuGetSepFromSysInfo(pFileDetails->fileSysInfo))
    {
        err = kNuErrLeadingFssep;
        goto bail;
    }

    DBUG(("+++ ADDING '%s' (%s) 0x%02lx 0x%04lx threadID=0x%08lx\n",
        pathnameUNI, pFileDetails->storageName, pFileDetails->fileType,
        pFileDetails->extraType, pFileDetails->threadID));

    /*
     * See if there's another record among the "new additions" with the
     * same storageName and compatible threads.
     *
     * If found, add a new thread in that record.  If an incompatibility
     * exists (same fork already present, disk image is there, etc), either
     * create a new record or return with an error.
     *
     * We want to search from the *end* of the "new" list, so that if
     * duplicates are allowed we find the entry most likely to be paired
     * up with the fork currently being added.
     */
    if (Nu_RecordSet_GetLoaded(&pArchive->newRecordSet)) {
        NuRecord* pNewRecord;

        err = Nu_RecordSet_ReverseFindByName(&pArchive->newRecordSet,
                pFileDetails->storageNameMOR, &pNewRecord);
        if (err == kNuErrNone) {
            /* is it okay to add it here? */
            err = Nu_OkayToAddThread(pArchive, pNewRecord,
                    pFileDetails->threadID);

            if (err == kNuErrNone) {
                /* okay to add it to this record */
                DBUG(("    attaching to existing record %06ld\n",
                    pNewRecord->recordIdx));
                err = Nu_AddFileThreadMod(pArchive, pNewRecord, pathnameUNI,
                        pFileDetails, fromRsrcFork);
                BailError(err);
                recordIdx = pNewRecord->recordIdx;
                goto bail;      /* we're done! */
            }

            err = kNuErrNone;   /* go a little farther */

            /*
             * We found a brand-new record with the same name, but we
             * can't add this fork to that record.  We can't delete the
             * item from the "new" list, so we can ignore HandleExisting.
             * If we don't allow duplicates, return an error; if we do,
             * then just continue with the normal processing path.
             */
            if (!pArchive->valAllowDuplicates) {
                DBUG(("+++ found matching record in new list, no dups\n"));
                err = kNuErrRecordExists;
                goto bail;
            }

        } else if (err == kNuErrRecNameNotFound) {
            /* no match in "new" list, fall through to normal processing */
            err = kNuErrNone;
        } else {
            /* general failure */
            goto bail;
        }
    }

    /*
     * Wasn't found, invoke Nu_AddRecord.  This will search through the
     * existing records, using the "allow duplicates" flag to cope with
     * any matches it finds.  On success, we should have a brand-new record
     * to play with.
     */
    err = Nu_AddRecord(pArchive, pFileDetails, &recordIdx, &pRecord);
    BailError(err);
    DBUG(("--- Added new record %06ld\n", recordIdx));

    /*
     * Got the record, now add a data file thread.
     */
    err = Nu_AddFileThreadMod(pArchive, pRecord, pathnameUNI, pFileDetails,
            fromRsrcFork);
    BailError(err);

bail:
    if (err == kNuErrNone && pRecordIdx != NULL)
        *pRecordIdx = recordIdx;

    return err;
}


/*
 * Rename a record.  There are three situations:
 *
 *  (1) Record has the filename in a thread, and the field has enough
 *   room to hold the new name.  For this case we add an "update" threadMod
 *   with the new data.
 *  (2) Record has the filename in a thread, and there is not enough room
 *   to hold the new name.  Here, we add a "delete" threadMod for the
 *   existing filename, and add an "add" threadMod for the new.
 *  (3) Record stores the filename in the header.  We zero out the filename
 *   and add a filename thread.
 *
 * We don't actually check to see if the filename is changing.  If you
 * want to rename something to the same thing, go right ahead.  (This
 * provides a way for applications to "filter" records that have filenames
 * in the headers instead of a thread.)
 *
 * BUG: we shouldn't allow a disk image to be renamed to have a complex
 * path name (e.g. "dir1:dir2:foo").  However, we may not be able to catch
 * that here depending on pending operations.
 *
 * We might also want to screen out trailing fssep chars, though the NuFX
 * spec doesn't say they're illegal.
 */
NuError Nu_Rename(NuArchive* pArchive, NuRecordIdx recIdx,
    const char* pathnameMOR, char fssepMOR)
{
    NuError err;
    NuRecord* pRecord;
    NuThread* pFilenameThread;
    const NuThreadMod* pThreadMod;
    NuThreadMod* pNewThreadMod = NULL;
    NuDataSource* pDataSource = NULL;
    long requiredCapacity, existingCapacity, newCapacity;
    Boolean doDelete, doAdd, doUpdate;

    if (recIdx == 0 || pathnameMOR == NULL || pathnameMOR[0] == '\0' ||
            fssepMOR == '\0')
    {
        return kNuErrInvalidArg;
    }

    if (pathnameMOR[0] == fssepMOR) {
        err = kNuErrLeadingFssep;
        Nu_ReportError(NU_BLOB, err, "rename path");
        goto bail;
    }

    if (Nu_IsReadOnly(pArchive))
        return kNuErrArchiveRO;
    err = Nu_GetTOCIfNeeded(pArchive);
    BailError(err);

    /* find the record in the "copy" set */
    err = Nu_FindRecordForWriteByIdx(pArchive, recIdx, &pRecord);
    BailError(err);
    Assert(pRecord != NULL);

    /* look for a filename thread */
    err = Nu_FindThreadByID(pRecord, kNuThreadIDFilename, &pFilenameThread);

    if (err != kNuErrNone)
        pFilenameThread = NULL;
    else if (err == kNuErrNone && pRecord->pThreadMods) {
        /* found a thread, check to see if it has been deleted (or modifed) */
        Assert(pFilenameThread != NULL);
        pThreadMod = Nu_ThreadMod_FindByThreadIdx(pRecord,
                        pFilenameThread->threadIdx);
        if (pThreadMod != NULL) {
            DBUG(("--- tried to modify threadIdx %ld, which has already been\n",
                pFilenameThread->threadIdx));
            err = kNuErrModThreadChange;
            goto bail;
        }
    }

    /*
     * Looks like we're okay so far.  Figure out what to do.
     */
    doDelete = doAdd = doUpdate = false;
    newCapacity = existingCapacity = 0;
    requiredCapacity = strlen(pathnameMOR);

    if (pFilenameThread != NULL) {
        existingCapacity = pFilenameThread->thCompThreadEOF;
        if (existingCapacity >= requiredCapacity) {
            doUpdate = true;
            newCapacity = existingCapacity;
        } else {
            doDelete = doAdd = true;
            /* make sure they have a few bytes of leeway */
            /*newCapacity = (requiredCapacity + kNuDefaultFilenameThreadSize) &
                                        (~(kNuDefaultFilenameThreadSize-1));*/
            newCapacity = requiredCapacity + 8;
        }
    } else {
        doAdd = true;
        /*newCapacity = (requiredCapacity + kNuDefaultFilenameThreadSize) &
                                        (~(kNuDefaultFilenameThreadSize-1));*/
        newCapacity = requiredCapacity + 8;
    }

    Assert(doAdd || doDelete || doUpdate);
    Assert(doDelete == false || doAdd == true);

    /* create a data source for the filename, if needed */
    if (doAdd || doUpdate) {
        Assert(newCapacity);
        err = Nu_DataSourceBuffer_New(kNuThreadFormatUncompressed,
                newCapacity, (const uint8_t*)strdup(pathnameMOR), 0,
                requiredCapacity /*(strlen)*/, Nu_InternalFreeCallback,
                &pDataSource);
        BailError(err);
    }

    if (doDelete) {
        err = Nu_ThreadModDelete_New(pArchive, pFilenameThread->threadIdx,
                kNuThreadIDFilename, &pNewThreadMod);
        BailError(err);
        Nu_RecordAddThreadMod(pRecord, pNewThreadMod);
        pNewThreadMod = NULL;    /* successful, don't free */
    }

    if (doAdd) {
        err = Nu_ThreadModAdd_New(pArchive, kNuThreadIDFilename,
                kNuThreadFormatUncompressed, pDataSource, &pNewThreadMod);
        BailError(err);
        /*pDataSource = NULL;*/  /* ThreadModAdd_New makes a copy */
        Nu_RecordAddThreadMod(pRecord, pNewThreadMod);
        pNewThreadMod = NULL;    /* successful, don't free */
    }

    if (doUpdate) {
        err = Nu_ThreadModUpdate_New(pArchive, pFilenameThread->threadIdx, 
                pDataSource, &pNewThreadMod);
        BailError(err);
        /*pDataSource = NULL;*/  /* ThreadModAdd_New makes a copy */
        Nu_RecordAddThreadMod(pRecord, pNewThreadMod);
        pNewThreadMod = NULL;    /* successful, don't free */
    }

    DBUG(("--- renaming '%s' to '%s' with delete=%d add=%d update=%d\n",
        pRecord->filenameMOR, pathnameMOR, doDelete, doAdd, doUpdate));

    /*
     * Update the fssep, if necessary.  (This is slightly silly -- we
     * have to rewrite the record header anyway since we're changing
     * threads around.)
     */
    if (NuGetSepFromSysInfo(pRecord->recFileSysInfo) != fssepMOR) {
        DBUG(("---  and updating the fssep\n"));
        pRecord->recFileSysInfo = NuSetSepInSysInfo(pRecord->recFileSysInfo,
                                    fssepMOR);
        pRecord->dirtyHeader = true;
    }

    /* if we had a header filename, mark it for oblivion */
    if (pFilenameThread == NULL) {
        DBUG(("+++ rename gonna drop the filename\n"));
        pRecord->dropRecFilename = true;
    }

bail:
    Nu_ThreadModFree(pArchive, pNewThreadMod);
    Nu_DataSourceFree(pDataSource);
    return err;
}


/*
 * Update a record's attributes with the contents of pRecordAttr.
 */
NuError Nu_SetRecordAttr(NuArchive* pArchive, NuRecordIdx recordIdx,
    const NuRecordAttr* pRecordAttr)
{
    NuError err;
    NuRecord* pRecord;

    if (pRecordAttr == NULL)
        return kNuErrInvalidArg;

    if (Nu_IsReadOnly(pArchive))
        return kNuErrArchiveRO;
    err = Nu_GetTOCIfNeeded(pArchive);
    BailError(err);

    /* pull the record out of the "copy" set */
    err = Nu_FindRecordForWriteByIdx(pArchive, recordIdx, &pRecord);
    BailError(err);

    Assert(pRecord != NULL);
    pRecord->recFileSysID = pRecordAttr->fileSysID;
    /*pRecord->recFileSysInfo = pRecordAttr->fileSysInfo;*/
    pRecord->recAccess = pRecordAttr->access;
    pRecord->recFileType = pRecordAttr->fileType;
    pRecord->recExtraType = pRecordAttr->extraType;
    pRecord->recCreateWhen = pRecordAttr->createWhen;
    pRecord->recModWhen = pRecordAttr->modWhen;
    pRecord->recArchiveWhen = pRecordAttr->archiveWhen;
    pRecord->dirtyHeader = true;

bail:
    return err;
}


/*
 * Bulk-delete several records, using the selection filter callback.
 */
NuError Nu_Delete(NuArchive* pArchive)
{
    NuError err;
    NuSelectionProposal selProposal;
    NuRecord* pNextRecord;
    NuRecord* pRecord;
    NuResult result;

    if (Nu_IsReadOnly(pArchive))
        return kNuErrArchiveRO;
    err = Nu_GetTOCIfNeeded(pArchive);
    BailError(err);

    /* 
     * If we don't yet have a copy set, make one.
     */
    if (!Nu_RecordSet_GetLoaded(&pArchive->copyRecordSet)) {
        err = Nu_RecordSet_Clone(pArchive, &pArchive->copyRecordSet,
                &pArchive->origRecordSet);
        BailError(err);
    }

    /*
     * Run through the copy set.  This is different from most other
     * operations, which run through the "orig" set.  However, since
     * we're not interested in allowing the user to delete things that
     * have already been deleted, we might as well use this set.
     */
    pNextRecord = Nu_RecordSet_GetListHead(&pArchive->copyRecordSet);
    while (pNextRecord != NULL) {
        pRecord = pNextRecord;
        pNextRecord = pRecord->pNext;

        /*
         * Deletion of modified records (thread adds, deletes, or updates)
         * isn't allowed.  There's no point in showing the record to the
         * user.
         */
        if (pRecord->pThreadMods != NULL) {
            DBUG(("+++ Skipping delete on a modified record\n"));
            continue;
        }

        /*
         * If a selection filter is defined, allow the user the opportunity
         * to select which files will be deleted, or abort the entire
         * operation.
         */
        if (pArchive->selectionFilterFunc != NULL) {
            selProposal.pRecord = pRecord;
            selProposal.pThread = pRecord->pThreads;    /* doesn't matter */
            result = (*pArchive->selectionFilterFunc)(pArchive, &selProposal);

            if (result == kNuSkip)
                continue;
            if (result == kNuAbort) {
                err = kNuErrAborted;
                goto bail;
            }
        }

        /*
         * Do we want to allow this?  (Same test as for DeleteRecord.)
         */
        if (pRecord->pThreadMods != NULL || pRecord->dirtyHeader) {
            DBUG(("--- Tried to delete a modified record\n"));
            err = kNuErrModRecChange;
            goto bail;
        }

        err = Nu_RecordSet_DeleteRecord(pArchive, &pArchive->copyRecordSet,
                pRecord);
        BailError(err);
    }

bail:
    return err;
}

/*
 * Delete an entire record.
 */
NuError Nu_DeleteRecord(NuArchive* pArchive, NuRecordIdx recIdx)
{
    NuError err;
    NuRecord* pRecord;

    if (Nu_IsReadOnly(pArchive))
        return kNuErrArchiveRO;
    err = Nu_GetTOCIfNeeded(pArchive);
    BailError(err);

    err = Nu_FindRecordForWriteByIdx(pArchive, recIdx, &pRecord);
    BailError(err);

    /*
     * Deletion of modified records (thread adds, deletes, or updates) isn't
     * allowed.  It probably wouldn't be hard to handle, but it's pointless.
     * Preventing the action maintains our general semantics of disallowing
     * conflicting actions on the same object.
     *
     * We also block it if the header is dirty (e.g. they changed the
     * record's filetype).  This isn't necessary for correct operation,
     * but again it maintains the semantics.
     */
    if (pRecord->pThreadMods != NULL || pRecord->dirtyHeader) {
        DBUG(("--- Tried to delete a modified record\n"));
        err = kNuErrModRecChange;
        goto bail;
    }

    err = Nu_RecordSet_DeleteRecord(pArchive,&pArchive->copyRecordSet, pRecord);
    BailError(err);

bail:
    return err;
}

