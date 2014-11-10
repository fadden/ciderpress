/*
 * NuFX archive manipulation library
 * Copyright (C) 2000-2007 by Andy McFadden, All Rights Reserved.
 * This is free software; you can redistribute it and/or modify it under the
 * terms of the BSD License, see the file COPYING-LIB.
 *
 * Debugging functions.  These are omitted from the non-debug build.
 */
#include "NufxLibPriv.h"

#if defined(DEBUG_MSGS)


/* pull a string out of one of the static arrays */
#define GetStaticString(index, staticArray) ( \
            (index) >= NELEM(staticArray) ? "<unknown>" : staticArray[index] \
        )

/* thread's thread_class */
static const char* gThreadClassNames[] = {
    "message_thread",
    "control_thread",
    "data_thread",
    "filename_thread",
};

/* thread's thread_format */
static const char* gThreadFormatNames[] = {
    "uncompressed",
    "Huffman Squeeze",
    "dynamic LZW/1",
    "dynamic LZW/2",
    "12-bit LZC",
    "16-bit LZC",
    "deflate",
    "bzip2"
};

/* days of the week */
static const char* gDayNames[] = {
    "[ null ]",
    "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
};
/* months of the year */
static const char* gMonths[] = {
    "Jan", "Feb", "Mar", "Apr", "May", "Jun",
    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};

#define kNuDateOutputLen    64

/* file_sys_id values */
static const char* gFileSysIDs[] = {
    "Reserved/unknown ($00)", "ProDOS/SOS", "DOS 3.3", "DOS 3.2",
    "Apple II Pascal", "Macintosh (HFS)", "Macintosh (MFS)",
    "LISA file system", "Apple CP/M", "Reserved 0x09", "MS-DOS",
    "High-Sierra", "ISO 9660", "AppleShare"
};


/*
 * Convert a DateTime structure into something printable.
 *
 * The buffer passed in must hold at least kNuDateOutputLen bytes.
 *
 * Returns "buffer" for the benefit of printf() calls.
 */
static char*
Nu_DebugDumpDate(const NuDateTime* pDateTime, char* buffer)
{
    char* cp;

    /* is it valid? */
    if (pDateTime->day > 30 || pDateTime->month > 11 || pDateTime->hour > 24 ||
        pDateTime->minute > 59)
    {
        strcpy(buffer, "   <invalid>   ");
        goto bail;
    }

    /* is it empty? */
    if ((pDateTime->second | pDateTime->minute | pDateTime->hour |
        pDateTime->year | pDateTime->day | pDateTime->month |
        pDateTime->extra | pDateTime->weekDay) == 0)
    {
        strcpy(buffer, "   [No Date]   ");
        goto bail;
    }

    cp = buffer;

    /* only print weekDay if one was stored */
    if (pDateTime->weekDay) {
        if (pDateTime->weekDay < NELEM(gDayNames))
            sprintf(cp, "%s, ", gDayNames[pDateTime->weekDay]);
        else
            sprintf(cp, "??%d, ", pDateTime->weekDay);
        cp += strlen(cp);
    }

    sprintf(cp, "%02d-%s-%04d  %02d:%02d:%02d",
        pDateTime->day+1, gMonths[pDateTime->month],
        pDateTime->year < 40 ? pDateTime->year + 2000 : pDateTime->year + 1900,
        pDateTime->hour, pDateTime->minute, pDateTime->second);

bail:
    sprintf(buffer + strlen(buffer), "  [s%d m%d h%d Y%d D%d M%d x%d w%d]",
        pDateTime->second, pDateTime->minute, pDateTime->hour,
        pDateTime->year, pDateTime->day, pDateTime->month, pDateTime->extra,
        pDateTime->weekDay);

    return buffer;
}


/*
 * Convert a buffer into a hexadecimal character string.
 *
 * The result will be 2x the size of the original, +1 for a null byte.
 */
static void
ConvertToHexStr(const uchar* inBuf, int inLen, char* outBuf)
{
    while (inLen--) {
        *outBuf++ = HexConv((*inBuf >> 4) & 0x0f);
        *outBuf++ = HexConv(*inBuf & 0x0f);
        inBuf++;
    }
    *outBuf = '\0';
}


/*
 * Dump everything we know about pThread.
 */
void
Nu_DebugDumpThread(const NuThread* pThread)
{
    static const char* kInd = "      ";
    NuThreadID threadID;
    const char* descr;

    Assert(pThread != nil);

    printf("%sThreadClass:  0x%04x (%s)\n", kInd,
        pThread->thThreadClass,
        GetStaticString(pThread->thThreadClass, gThreadClassNames));
    printf("%sThreadFormat: 0x%04x (%s)\n", kInd,
        pThread->thThreadFormat,
        GetStaticString(pThread->thThreadFormat, gThreadFormatNames));

    threadID = NuMakeThreadID(pThread->thThreadClass, pThread->thThreadKind);
    switch (threadID) {
    case kNuThreadIDOldComment: descr = "old comment";  break;
    case kNuThreadIDComment:    descr = "comment";      break;
    case kNuThreadIDIcon:       descr = "icon";         break;
    case kNuThreadIDMkdir:      descr = "mkdir";        break;
    case kNuThreadIDDataFork:   descr = "data fork";    break;
    case kNuThreadIDDiskImage:  descr = "disk image";   break;
    case kNuThreadIDRsrcFork:   descr = "rsrc fork";    break;
    case kNuThreadIDFilename:   descr = "filename";     break;
    default:                    descr = "<unknown>";    break;
    }
    printf("%sThreadKind:   0x%04x (%s)\n", kInd,
        pThread->thThreadKind, descr);

    printf("%sThreadCRC: 0x%04x  ThreadEOF: %lu  CompThreadEOF: %lu\n", kInd,
        pThread->thThreadCRC, pThread->thThreadEOF, pThread->thCompThreadEOF);
    printf("%s*File data offset: %ld  actualThreadEOF: %ld\n", kInd,
        pThread->fileOffset, pThread->actualThreadEOF);
}

/*
 * Dump everything we know about pRecord, including its threads and ThreadMods.
 *
 * Changes to existing records are made to the "copy" set, not the "orig"
 * set.  Pass in the "orig" copy in "pRecord", and optionally pass in the
 * "copy" set in "pXrefRecord" to glean data from both.
 */
static void
Nu_DebugDumpRecord(NuArchive* pArchive, const NuRecord* pRecord,
    const NuRecord* pXrefRecord, Boolean isDeleted)
{
    NuError err;    /* dummy */
    static const char* kInd = "    ";
    char dateBuf[kNuDateOutputLen];
    const NuThreadMod* pThreadMod;
    const NuThread* pThread;
    ulong idx;

    Assert(pRecord != nil);

    /*printf("PTR: pRecord=0x%08lx pXrefRecord=0x%08lx\n", (long) pRecord,
        (long) pXrefRecord);*/

    printf("%s%s%sFilename: '%s' (idx=%lu)\n", kInd,
        isDeleted ? "[DEL] " : "",
        pXrefRecord != nil && pXrefRecord->pThreadMods != nil ? "[MOD] " : "",
        pRecord->filename == nil ? "<not specified>" : pRecord->filename,
        pRecord->recordIdx);
    printf("%sHeaderID: '%.4s'  VersionNumber: 0x%04x  HeaderCRC: 0x%04x\n",
        kInd,
        pRecord->recNufxID, pRecord->recVersionNumber, pRecord->recHeaderCRC);
    printf("%sAttribCount: %u  TotalThreads: %lu\n", kInd,
        pRecord->recAttribCount, pRecord->recTotalThreads);
    printf("%sFileSysID: %u (%s)  FileSysInfo: 0x%04x ('%c')\n", kInd,
        pRecord->recFileSysID,
        GetStaticString(pRecord->recFileSysID, gFileSysIDs),
        pRecord->recFileSysInfo,
        NuGetSepFromSysInfo(pRecord->recFileSysInfo));
    /* do something fancy for ProDOS? */
    printf("%sFileType: 0x%08lx  ExtraType: 0x%08lx  Access: 0x%08lx\n", kInd,
        pRecord->recFileType, pRecord->recExtraType, pRecord->recAccess);
    printf("%sCreateWhen:  %s\n", kInd,
        Nu_DebugDumpDate(&pRecord->recCreateWhen, dateBuf));
    printf("%sModWhen:     %s\n", kInd,
        Nu_DebugDumpDate(&pRecord->recModWhen, dateBuf));
    printf("%sArchiveWhen: %s\n", kInd,
        Nu_DebugDumpDate(&pRecord->recArchiveWhen, dateBuf));
    printf("%sStorageType: %u  OptionSize: %u  FilenameLength: %u\n", kInd,
        pRecord->recStorageType, pRecord->recOptionSize,
        pRecord->recFilenameLength);
    if (pRecord->recOptionSize) {
        char* outBuf = Nu_Malloc(pArchive, pRecord->recOptionSize * 2 +1);
        BailAlloc(outBuf);
        Assert(pRecord->recOptionList != nil);
        ConvertToHexStr(pRecord->recOptionList, pRecord->recOptionSize, outBuf);
        printf("%sOptionList: [%s]\n", kInd, outBuf);
        Nu_Free(pArchive, outBuf);
    }

    printf("%s*ExtraCount: %ld  RecFileOffset: %ld  RecHeaderLength: %ld\n",
        kInd,
        pRecord->extraCount, pRecord->fileOffset, pRecord->recHeaderLength);

    for (idx = 0; idx < pRecord->recTotalThreads; idx++) {
        Boolean isFake;

        isFake = (idx >= pRecord->recTotalThreads - pRecord->fakeThreads);
        pThread = Nu_GetThread(pRecord, idx);
        Assert(pThread != nil);

        printf("%s--Thread #%lu (idx=%lu)%s\n", kInd, idx, pThread->threadIdx,
            isFake ? " [FAKE]" : "");
        Nu_DebugDumpThread(pThread);
    }

    if (pXrefRecord != nil)
        pThreadMod = pXrefRecord->pThreadMods;
    else
        pThreadMod = pRecord->pThreadMods;  /* probably empty */

    if (pThreadMod != nil)
        printf("%s*ThreadMods -----\n", kInd);
    while (pThreadMod != nil) {
        switch (pThreadMod->entry.kind) {
        case kNuThreadModAdd:
            printf("%s  *-ThreadMod ADD 0x%08lx 0x%04x (sourceType=%d)\n", kInd,
                pThreadMod->entry.add.threadID,
                pThreadMod->entry.add.threadFormat,
                Nu_DataSourceGetType(pThreadMod->entry.add.pDataSource));
            break;
        case kNuThreadModUpdate:
            printf("%s  *-ThreadMod UPDATE %6ld\n", kInd,
                pThreadMod->entry.update.threadIdx);
            break;
        case kNuThreadModDelete:
            printf("%s  *-ThreadMod DELETE %6ld\n", kInd,
                pThreadMod->entry.delete.threadIdx);
            break;
        case kNuThreadModUnknown:
        default:
            Assert(0);
            printf("%s++ThreadMod UNKNOWN\n", kInd);
            break;
        }

        pThreadMod = pThreadMod->pNext;
    }

    /*printf("%s*TotalLength: %ld  TotalCompLength: %ld\n",
        kInd, pRecord->totalLength, pRecord->totalCompLength);*/
    printf("%s*TotalCompLength: %ld\n", kInd, pRecord->totalCompLength);
    printf("\n");

bail:
    return;
}

/*
 * Dump the records in a RecordSet.
 */
static void
Nu_DebugDumpRecordSet(NuArchive* pArchive, const NuRecordSet* pRecordSet,
    const NuRecordSet* pXrefSet)
{
    const NuRecord* pRecord;
    const NuRecord* pXrefRecord;
    Boolean doXref;
    long count;

    doXref = false;
    pXrefRecord = nil;
    if (pXrefSet != nil && Nu_RecordSet_GetLoaded(pXrefSet)) {
        pXrefRecord = Nu_RecordSet_GetListHead(pXrefSet);
        doXref = true;
    }

    /* dump every record, if we've loaded them */
    count = Nu_RecordSet_GetNumRecords(pRecordSet);
    pRecord = Nu_RecordSet_GetListHead(pRecordSet);
    if (pRecord != nil) {
        Assert(count != 0);
        while (count--) {
            Assert(pRecord != nil);

            if (pXrefRecord != nil &&
                pRecord->recordIdx == pXrefRecord->recordIdx)
            {
                Nu_DebugDumpRecord(pArchive, pRecord, pXrefRecord, false);
                pXrefRecord = pXrefRecord->pNext;
            } else {
                Nu_DebugDumpRecord(pArchive, pRecord, nil, doXref);
            }
            pRecord = pRecord->pNext;
        }
    } else {
        Assert(count == 0);
    }
}

/*
 * Dump the master header block.
 */
static void
Nu_DebugDumpMH(const NuMasterHeader* pMasterHeader)
{
    static const char* kInd = "  ";
    char dateBuf1[kNuDateOutputLen];

    Assert(pMasterHeader != nil);

    printf("%sNufileID: '%.6s'  MasterCRC: 0x%04x  TotalRecords: %lu\n", kInd,
        pMasterHeader->mhNufileID, pMasterHeader->mhMasterCRC,
        pMasterHeader->mhTotalRecords);
    printf("%sArchiveCreateWhen: %s\n", kInd,
        Nu_DebugDumpDate(&pMasterHeader->mhArchiveCreateWhen, dateBuf1));
    printf("%sArchiveModWhen:    %s\n", kInd,
        Nu_DebugDumpDate(&pMasterHeader->mhArchiveModWhen, dateBuf1));
    printf("%sMasterVersion: %u  MasterEOF: %lu\n", kInd,
        pMasterHeader->mhMasterVersion, pMasterHeader->mhMasterEOF);
}

/*
 * Dump everything we know about pArchive.
 *
 * This will only print the records that we have seen so far.  If the
 * application hasn't caused us to scan through all of the records in
 * the archive, then this won't be very interesting.  This will never
 * show any records for streaming-mode archives.
 */
void
Nu_DebugDumpAll(NuArchive* pArchive)
{
    Assert(pArchive != nil);

    printf("*Archive pathname: '%s'\n", pArchive->archivePathname);
    printf("*Archive type: %d\n", pArchive->archiveType);
    printf("*Header offset: %ld (junk offset=%ld)\n",
        pArchive->headerOffset, pArchive->junkOffset);
    printf("*Num records: %ld orig, %ld copy, %ld new\n",
        Nu_RecordSet_GetNumRecords(&pArchive->origRecordSet),
        Nu_RecordSet_GetNumRecords(&pArchive->copyRecordSet),
        Nu_RecordSet_GetNumRecords(&pArchive->newRecordSet));
    printf("*NuRecordIdx seed: %lu  NuRecordIdx next: %lu\n",
        pArchive->recordIdxSeed, pArchive->nextRecordIdx);

    /* master header */
    Nu_DebugDumpMH(&pArchive->masterHeader);

    printf("  *ORIG record set (x-ref with COPY):\n");
    Nu_DebugDumpRecordSet(pArchive, &pArchive->origRecordSet,
        &pArchive->copyRecordSet);
    printf("  *NEW record set:\n");
    Nu_DebugDumpRecordSet(pArchive, &pArchive->newRecordSet, nil);

    if (!Nu_RecordSet_GetLoaded(&pArchive->origRecordSet) &&
        !Nu_RecordSet_GetLoaded(&pArchive->newRecordSet))
    {
        printf("*** DEBUG: original records not loaded yet? ***\n");
    }

}

#endif /*DEBUG_MSGS*/
