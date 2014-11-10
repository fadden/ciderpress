/*
 * NuFX archive manipulation library
 * Copyright (C) 2000-2007 by Andy McFadden, All Rights Reserved.
 * This is free software; you can redistribute it and/or modify it under the
 * terms of the BSD License, see the file COPYING-LIB.
 *
 * Archive structure creation and manipulation.
 */
#include "NufxLibPriv.h"

#ifdef HAVE_FCNTL_H
# include <fcntl.h>
#endif
#ifndef O_BINARY
# define O_BINARY   0
#endif

/* master header identification */
static const uchar kNuMasterID[kNufileIDLen] =
    { 0x4e, 0xf5, 0x46, 0xe9, 0x6c, 0xe5 };

/* other identification; can be no longer than kNufileIDLen */
static const uchar kNuBinary2ID[] =
    { 0x0a, 0x47, 0x4c };
static const uchar kNuSHKSEAID[] =
    { 0xa2, 0x2e, 0x00 };

/*
 * Offsets to some interesting places in the wrappers.
 */
#define kNuBNYFileSizeLo    8       /* file size in 512-byte blocks (2B) */
#define kNuBNYFileSizeHi    114     /*  ... (2B) */
#define kNuBNYEOFLo         20      /* file size in bytes (3B) */
#define kNuBNYEOFHi         116     /*  ... (1B) */
#define kNuBNYDiskSpace     117     /* total space req'd; equiv FileSize (4B) */
#define kNuBNYFilesToFollow 127     /* (1B) #of files in rest of BNY file */
#define kNuSEAFunkySize     11938   /* length of archive + 68 (4B?) */
#define kNuSEAFunkyAdjust   68      /*  ... adjustment to "FunkySize" */
#define kNuSEALength1       11946   /* length of archive (4B?) */
#define kNuSEALength2       12001   /* length of archive (4B?) */

#define kDefaultJunkSkipMax 1024    /* default junk scan size */

static void Nu_CloseAndFree(NuArchive* pArchive);


/*
 * ===========================================================================
 *      Archive and MasterHeader utility functions
 * ===========================================================================
 */

/*
 * Allocate and initialize a new NuArchive structure.
 */
static NuError
Nu_NuArchiveNew(NuArchive** ppArchive)
{
    Assert(ppArchive != nil);

    /* validate some assumptions we make throughout the code */
    Assert(sizeof(int) >= 2);
    Assert(sizeof(ushort) >= 2);
    Assert(sizeof(ulong) >= 4);
    Assert(sizeof(void*) >= sizeof(NuArchive*));

    *ppArchive = Nu_Calloc(nil, sizeof(**ppArchive));
    if (*ppArchive == nil)
        return kNuErrMalloc;

    (*ppArchive)->structMagic = kNuArchiveStructMagic;

    (*ppArchive)->recordIdxSeed = 1000; /* could be a random number */
    (*ppArchive)->nextRecordIdx = (*ppArchive)->recordIdxSeed;

    /*
     * Initialize assorted values to defaults.  We don't try to do any
     * system-specific values here; it's up to the application to decide
     * what is most appropriate for the current system.
     */
    (*ppArchive)->valIgnoreCRC = false;
    #ifdef ENABLE_LZW
    (*ppArchive)->valDataCompression = kNuCompressLZW2;
    #else
    (*ppArchive)->valDataCompression = kNuCompressNone;
    #endif
    (*ppArchive)->valDiscardWrapper = false;
    (*ppArchive)->valEOL = kNuEOLLF;    /* non-UNIX apps must override */
    (*ppArchive)->valConvertExtractedEOL = kNuConvertOff;
    (*ppArchive)->valOnlyUpdateOlder = false;
    (*ppArchive)->valAllowDuplicates = false;
    (*ppArchive)->valHandleExisting = kNuMaybeOverwrite;
    (*ppArchive)->valModifyOrig = false;
    (*ppArchive)->valMimicSHK = false;
    (*ppArchive)->valMaskDataless = false;
    (*ppArchive)->valStripHighASCII = false;
    /* bug: this can't be set by application! */
    (*ppArchive)->valJunkSkipMax = kDefaultJunkSkipMax;
    (*ppArchive)->valIgnoreLZW2Len = false;
    (*ppArchive)->valHandleBadMac = false;

    (*ppArchive)->messageHandlerFunc = gNuGlobalErrorMessageHandler;

    return kNuErrNone;
}

/*
 * Free up a NuArchive structure and its contents.
 */
static NuError
Nu_NuArchiveFree(NuArchive* pArchive)
{
    Assert(pArchive != nil);
    Assert(pArchive->structMagic == kNuArchiveStructMagic);

    (void) Nu_RecordSet_FreeAllRecords(pArchive, &pArchive->origRecordSet);
    pArchive->haveToc = false;
    (void) Nu_RecordSet_FreeAllRecords(pArchive, &pArchive->copyRecordSet);
    (void) Nu_RecordSet_FreeAllRecords(pArchive, &pArchive->newRecordSet);

    Nu_Free(nil, pArchive->archivePathname);
    Nu_Free(nil, pArchive->tmpPathname);
    Nu_Free(nil, pArchive->compBuf);
    Nu_Free(nil, pArchive->lzwCompressState);
    Nu_Free(nil, pArchive->lzwExpandState);

    /* mark it as deceased to prevent further use, then free it */
    pArchive->structMagic = kNuArchiveStructMagic ^ 0xffffffff;
    Nu_Free(nil, pArchive);

    return kNuErrNone;
}


/*
 * Copy a NuMasterHeader struct.
 */
void
Nu_MasterHeaderCopy(NuArchive* pArchive, NuMasterHeader* pDstHeader,
    const NuMasterHeader* pSrcHeader)
{
    Assert(pArchive != nil);
    Assert(pDstHeader != nil);
    Assert(pSrcHeader != nil);

    *pDstHeader = *pSrcHeader;
}

/*
 * Get a pointer to the archive master header (this is an API call).
 */
NuError
Nu_GetMasterHeader(NuArchive* pArchive, const NuMasterHeader** ppMasterHeader)
{
    if (ppMasterHeader == nil)
        return kNuErrInvalidArg;

    *ppMasterHeader = &pArchive->masterHeader;

    return kNuErrNone;
}


/*
 * Allocate the general-purpose compression buffer, if needed.
 */
NuError
Nu_AllocCompressionBufferIFN(NuArchive* pArchive)
{
    Assert(pArchive != nil);

    if (pArchive->compBuf != nil)
        return kNuErrNone;

    pArchive->compBuf = Nu_Malloc(pArchive, kNuGenCompBufSize);
    if (pArchive->compBuf == nil)
        return kNuErrMalloc;

    return kNuErrNone;
}


/*
 * Return a unique value.
 */
NuRecordIdx
Nu_GetNextRecordIdx(NuArchive* pArchive)
{
    return pArchive->nextRecordIdx++;
}

/*
 * Return a unique value.
 */
NuThreadIdx
Nu_GetNextThreadIdx(NuArchive* pArchive)
{
    return pArchive->nextRecordIdx++;       /* just use the record counter */
}


/*
 * ===========================================================================
 *      Wrapper (SEA, BXY, BSE) functions
 * ===========================================================================
 */

/*
 * Copy the wrapper from the archive file to the temp file.
 */
NuError
Nu_CopyWrapperToTemp(NuArchive* pArchive)
{
    NuError err;

    Assert(pArchive->headerOffset);     /* no wrapper to copy?? */

    err = Nu_FSeek(pArchive->archiveFp, 0, SEEK_SET);
    BailError(err);
    err = Nu_FSeek(pArchive->tmpFp, 0, SEEK_SET);
    BailError(err);
    err = Nu_CopyFileSection(pArchive, pArchive->tmpFp,
            pArchive->archiveFp, pArchive->headerOffset);
    BailError(err);

bail:
    return err;
}


/*
 * Fix up the wrapper.  The SEA and BXY headers have some fields
 * set according to file length and archive attributes.
 *
 * Pass in the file pointer that will be written to.  Wrappers are
 * assumed to start at offset 0.
 *
 * Wrappers must appear in this order:
 *  Leading junk
 *  Binary II
 *  ShrinkIt SEA (Self-Extracting Archive)
 *
 * If they didn't, we wouldn't be this far.
 *
 * I have a Binary II specification, but don't have one for SEA, so I'm
 * making educated guesses based on the differences between archives.  I'd
 * guess some of the SEA weirdness stems from some far-sighted support
 * for multiple archives within a single SEA wrapper.
 */
NuError
Nu_UpdateWrapper(NuArchive* pArchive, FILE* fp)
{
    NuError err = kNuErrNone;
    Boolean hasBinary2, hasSea;
    uchar identBuf[kNufileIDLen];
    ulong archiveLen, archiveLen512;

    Assert(pArchive->newMasterHeader.isValid);  /* need new crc and len */

    hasBinary2 = hasSea = false;

    switch (pArchive->archiveType) {
    case kNuArchiveNuFX:
        goto bail;
    case kNuArchiveNuFXInBNY:
        hasBinary2 = true;
        break;
    case kNuArchiveNuFXSelfEx:
        hasSea = true;
        break;
    case kNuArchiveNuFXSelfExInBNY:
        hasBinary2 = hasSea = true;
        break;
    default:
        if (pArchive->headerOffset != 0 &&
            pArchive->headerOffset != pArchive->junkOffset)
        {
            Nu_ReportError(NU_BLOB, kNuErrNone, "Can't fix the wrapper??");
            err = kNuErrInternal;
            goto bail;
        } else
            goto bail;
    }

    err = Nu_FSeek(fp, pArchive->junkOffset, SEEK_SET);
    BailError(err);

    if (hasBinary2) {
        /* sanity check - make sure it's Binary II */
        Nu_ReadBytes(pArchive, fp, identBuf, kNufileIDLen);
        if ((err = Nu_HeaderIOFailed(pArchive, fp)) != kNuErrNone) {
            Nu_ReportError(NU_BLOB, err, "Failed reading BNY wrapper");
            goto bail;
        }
        if (memcmp(identBuf, kNuBinary2ID, sizeof(kNuBinary2ID)) != 0) {
            err = kNuErrInternal;
            Nu_ReportError(NU_BLOB, kNuErrNone,"Didn't find Binary II wrapper");
            goto bail;
        }

        /* archiveLen includes the SEA wrapper, if any, but excludes junk */
        archiveLen = pArchive->newMasterHeader.mhMasterEOF +
                        (pArchive->headerOffset - pArchive->junkOffset) -
                        kNuBinary2BlockSize;
        archiveLen512 = (archiveLen + 511) / 512;

        err = Nu_FSeek(fp, kNuBNYFileSizeLo - kNufileIDLen, SEEK_CUR);
        BailError(err);
        Nu_WriteTwo(pArchive, fp, (ushort)(archiveLen512 & 0xffff));

        err = Nu_FSeek(fp, kNuBNYFileSizeHi - (kNuBNYFileSizeLo+2), SEEK_CUR);
        BailError(err);
        Nu_WriteTwo(pArchive, fp, (ushort)(archiveLen512 >> 16));

        err = Nu_FSeek(fp, kNuBNYEOFLo - (kNuBNYFileSizeHi+2), SEEK_CUR);
        BailError(err);
        Nu_WriteTwo(pArchive, fp, (ushort)(archiveLen & 0xffff));
        Nu_WriteOne(pArchive, fp, (uchar)((archiveLen >> 16) & 0xff));

        err = Nu_FSeek(fp, kNuBNYEOFHi - (kNuBNYEOFLo+3), SEEK_CUR);
        BailError(err);
        Nu_WriteOne(pArchive, fp, (uchar)(archiveLen >> 24));

        err = Nu_FSeek(fp, kNuBNYDiskSpace - (kNuBNYEOFHi+1), SEEK_CUR);
        BailError(err);
        Nu_WriteFour(pArchive, fp, archiveLen512);

        /* probably ought to update "modified when" date/time field */

        /* seek just past end of BNY wrapper */
        err = Nu_FSeek(fp, kNuBinary2BlockSize - (kNuBNYDiskSpace+4), SEEK_CUR);
        BailError(err);

        if ((err = Nu_HeaderIOFailed(pArchive, fp)) != kNuErrNone) {
            Nu_ReportError(NU_BLOB, err, "Failed updating Binary II wrapper");
            goto bail;
        }
    }

    if (hasSea) {
        /* sanity check - make sure it's SEA */
        Nu_ReadBytes(pArchive, fp, identBuf, kNufileIDLen);
        if ((err = Nu_HeaderIOFailed(pArchive, fp)) != kNuErrNone) {
            Nu_ReportError(NU_BLOB, err, "Failed reading SEA wrapper");
            goto bail;
        }
        if (memcmp(identBuf, kNuSHKSEAID, sizeof(kNuSHKSEAID)) != 0) {
            err = kNuErrInternal;
            Nu_ReportError(NU_BLOB, kNuErrNone, "Didn't find SEA wrapper");
            goto bail;
        }

        archiveLen = pArchive->newMasterHeader.mhMasterEOF;

        err = Nu_FSeek(fp, kNuSEAFunkySize - kNufileIDLen, SEEK_CUR);
        BailError(err);
        Nu_WriteFour(pArchive, fp, archiveLen + kNuSEAFunkyAdjust);

        err = Nu_FSeek(fp, kNuSEALength1 - (kNuSEAFunkySize+4), SEEK_CUR);
        BailError(err);
        Nu_WriteTwo(pArchive, fp, (ushort)archiveLen);

        err = Nu_FSeek(fp, kNuSEALength2 - (kNuSEALength1+2), SEEK_CUR);
        BailError(err);
        Nu_WriteTwo(pArchive, fp, (ushort)archiveLen);

        /* seek past end of SEA wrapper */
        err = Nu_FSeek(fp, kNuSEAOffset - (kNuSEALength2+2), SEEK_CUR);
        BailError(err);

        if ((err = Nu_HeaderIOFailed(pArchive, fp)) != kNuErrNone) {
            Nu_ReportError(NU_BLOB, err, "Failed updating SEA wrapper");
            goto bail;
        }
    }

bail:
    return kNuErrNone;
}


/*
 * Adjust wrapper-induced padding on the archive.
 *
 * GS/ShrinkIt v1.1 does some peculiar things with SEA (Self-Extracting
 * Archive) files.  For no apparent reason, it always adds one extra 00
 * byte to the end.  When you combine SEA and BXY to make BSE, it will
 * leave that extra byte inside the BXY 128-byte padding area, UNLESS
 * the archive itself happens to be exactly 128 bytes, in which case
 * it throws the pad byte onto the end -- resulting in an archive that
 * isn't an exact multiple of 128.
 *
 * I've chosen to emulate the 1-byte padding "feature" of GSHK, but I'm
 * not going to try to emulate the quirky behavior described above.
 *
 * The SEA pad byte is added first, and then the 128-byte BXY padding
 * is considered.  In the odd case described above, the file would be
 * 127 bytes larger with nufxlib than it is with GSHK.  This shouldn't
 * require additional disk space to be used, assuming a filesystem block
 * size of at least 128 bytes.
 */
NuError
Nu_AdjustWrapperPadding(NuArchive* pArchive, FILE* fp)
{
    NuError err = kNuErrNone;
    Boolean hasBinary2, hasSea;

    hasBinary2 = hasSea = false;

    switch (pArchive->archiveType) {
    case kNuArchiveNuFX:
        goto bail;
    case kNuArchiveNuFXInBNY:
        hasBinary2 = true;
        break;
    case kNuArchiveNuFXSelfEx:
        hasSea = true;
        break;
    case kNuArchiveNuFXSelfExInBNY:
        hasBinary2 = hasSea = true;
        break;
    default:
        if (pArchive->headerOffset != 0 &&
            pArchive->headerOffset != pArchive->junkOffset)
        {
            Nu_ReportError(NU_BLOB, kNuErrNone, "Can't check the padding??");
            err = kNuErrInternal;
            goto bail;
        } else
            goto bail;
    }

    err = Nu_FSeek(fp, 0, SEEK_END);
    BailError(err);

    if (hasSea && pArchive->valMimicSHK) {
        /* throw on a single pad byte, for no apparent reason whatsoever */
        Nu_WriteOne(pArchive, fp, 0);
    }

    if (hasBinary2) {
        /* pad out to the next 128-byte boundary */
        long curOffset;

        err = Nu_FTell(fp, &curOffset);
        BailError(err);
        curOffset -= pArchive->junkOffset;  /* don't factor junk into account */

        DBUG(("+++ BNY needs %ld bytes of padding\n", curOffset & 0x7f));
        if (curOffset & 0x7f) {
            int i;

            for (i = kNuBinary2BlockSize - (curOffset & 0x7f); i > 0; i--)
                Nu_WriteOne(pArchive, fp, 0);
        }
    }

    if ((err = Nu_HeaderIOFailed(pArchive, fp)) != kNuErrNone) {
        Nu_ReportError(NU_BLOB, err, "Failed updating wrapper padding");
        goto bail;
    }

bail:
    return err;
}


/*
 * ===========================================================================
 *      Open an archive
 * ===========================================================================
 */

/*
 * Read the master header from the archive file.
 *
 * This also handles skipping the first 128 bytes of a .BXY file and the
 * front part of a self-extracting GSHK archive.
 *
 * We try to provide helpful messages about things that aren't archives,
 * but try to stay silent about files that are other types of archives.
 * That way, if the application is trying a series of libraries to find
 * one that will accept the file, we don't generate spurious complaints.
 *
 * Since there's a fair possibility that whoever is opening this file is
 * also interested in related formats, we try to return a meaningful error
 * code for stuff we recognize (especially Binary II).
 *
 * If at first we don't succeed, we keep trying further along until we
 * find something we recognize.  We don't want to just scan for the
 * NuFile ID, because that might prevent this from working properly with
 * SEA archives which push the NuFX start out about 12K.  We also wouldn't
 * be able to update the BNY/SEA wrappers correctly.  So, we inch our way
 * along until we find something we recognize or get bored.
 *
 * On exit, the stream will be positioned just past the master header.
 */
static NuError
Nu_ReadMasterHeader(NuArchive* pArchive)
{
    NuError err;
    ushort crc;
    FILE* fp;
    NuMasterHeader* pHeader;
    Boolean isBinary2 = false;
    Boolean isSea = false;

    Assert(pArchive != nil);

    fp = pArchive->archiveFp;       /* saves typing */
    pHeader = &pArchive->masterHeader;

    pArchive->junkOffset = 0;

retry:
    pArchive->headerOffset = pArchive->junkOffset;
    Nu_ReadBytes(pArchive, fp, pHeader->mhNufileID, kNufileIDLen);
    /* may have read fewer than kNufileIDLen; that's okay */

    if (memcmp(pHeader->mhNufileID, kNuBinary2ID, sizeof(kNuBinary2ID)) == 0)
    {
        int count;

        /* looks like a Binary II archive, might be BXY or BSE; seek forward */
        err = Nu_SeekArchive(pArchive, fp, kNuBNYFilesToFollow - kNufileIDLen,
                SEEK_CUR);
        if (err != kNuErrNone) {
            err = kNuErrNotNuFX;
            /* probably too short to be BNY, so go ahead and whine */
            Nu_ReportError(NU_BLOB, kNuErrNone,
                "Looks like a truncated Binary II archive?");
            goto bail;
        }

        /*
         * Check "files to follow", so we can be sure this isn't a BNY that
         * just happened to have a .SHK as the first file.  If it is, then
         * any updates to the archive will trash the rest of the BNY files.
         */
        count = Nu_ReadOne(pArchive, fp);
        if (count != 0) {
            err = kNuErrIsBinary2;
            /*Nu_ReportError(NU_BLOB, kNuErrNone,
                "This is a Binary II archive with %d files in it", count+1);*/
            DBUG(("This is a Binary II archive with %d files in it\n",count+1));
            goto bail;
        }

        /* that was last item in BNY header, no need to seek */
        Assert(kNuBNYFilesToFollow == kNuBinary2BlockSize -1);

        isBinary2 = true;
        pArchive->headerOffset += kNuBinary2BlockSize;
        Nu_ReadBytes(pArchive, fp, pHeader->mhNufileID, kNufileIDLen);
    }
    if (memcmp(pHeader->mhNufileID, kNuSHKSEAID, sizeof(kNuSHKSEAID)) == 0)
    {
        /* might be GSHK self-extracting; seek forward */
        err = Nu_SeekArchive(pArchive, fp, kNuSEAOffset - kNufileIDLen,
                SEEK_CUR);
        if (err != kNuErrNone) {
            err = kNuErrNotNuFX;
            Nu_ReportError(NU_BLOB, kNuErrNone,
                "Looks like GS executable, not NuFX");
            goto bail;
        }

        isSea = true;
        pArchive->headerOffset += kNuSEAOffset;
        Nu_ReadBytes(pArchive, fp, pHeader->mhNufileID, kNufileIDLen);
    }

    if (memcmp(kNuMasterID, pHeader->mhNufileID, kNufileIDLen) != 0) {
        /*
         * Doesn't look like a NuFX archive.  Scan forward and see if we
         * can find the start past some leading junk.  MacBinary headers
         * and chunks of HTTP seem popular on FTP sites.
         */
        if ((pArchive->openMode == kNuOpenRO ||
             pArchive->openMode == kNuOpenRW) &&
            pArchive->junkOffset < (long)pArchive->valJunkSkipMax)
        {
            pArchive->junkOffset++;
            DBUG(("+++ scanning from offset %ld\n", pArchive->junkOffset));
            err = Nu_SeekArchive(pArchive, fp, pArchive->junkOffset, SEEK_SET);
            BailError(err);
            goto retry;
        }

        err = kNuErrNotNuFX;

        if (isBinary2) {
            err = kNuErrIsBinary2;
            /*Nu_ReportError(NU_BLOB, kNuErrNone,
                "Looks like Binary II, not NuFX");*/
            DBUG(("Looks like Binary II, not NuFX\n"));
        } else if (isSea)
            Nu_ReportError(NU_BLOB, kNuErrNone,
                "Looks like GS executable, not NuFX");
        else if (Nu_HeaderIOFailed(pArchive, fp) != kNuErrNone)
            Nu_ReportError(NU_BLOB, kNuErrNone,
                "Couldn't read enough data, not NuFX?");
        else
            Nu_ReportError(NU_BLOB, kNuErrNone,
                "Not a NuFX archive?  Got 0x%02x%02x%02x%02x%02x%02x...",
                pHeader->mhNufileID[0], pHeader->mhNufileID[1],
                pHeader->mhNufileID[2], pHeader->mhNufileID[3],
                pHeader->mhNufileID[4], pHeader->mhNufileID[5]);
        goto bail;
    }

    if (pArchive->junkOffset != 0) {
        DBUG(("+++ found apparent start of archive at offset %ld\n",
            pArchive->junkOffset));
    }

    crc = 0;
    pHeader->mhMasterCRC = Nu_ReadTwo(pArchive, fp);
    pHeader->mhTotalRecords = Nu_ReadFourC(pArchive, fp, &crc);
    pHeader->mhArchiveCreateWhen = Nu_ReadDateTimeC(pArchive, fp, &crc);
    pHeader->mhArchiveModWhen = Nu_ReadDateTimeC(pArchive, fp, &crc);
    pHeader->mhMasterVersion = Nu_ReadTwoC(pArchive, fp, &crc);
    Nu_ReadBytesC(pArchive, fp, pHeader->mhReserved1,
                        kNufileMasterReserved1Len, &crc);
    pHeader->mhMasterEOF = Nu_ReadFourC(pArchive, fp, &crc);
    Nu_ReadBytesC(pArchive, fp, pHeader->mhReserved2,
                        kNufileMasterReserved2Len, &crc);

    /* check for errors in any of the above reads */
    if ((err = Nu_HeaderIOFailed(pArchive, fp)) != kNuErrNone) {
        Nu_ReportError(NU_BLOB, err, "Failed reading master header");
        goto bail;
    }
    if (pHeader->mhMasterVersion > kNuMaxMHVersion) {
        err = kNuErrBadMHVersion;
        Nu_ReportError(NU_BLOB, err, "Bad Master Header version %u",
            pHeader->mhMasterVersion);
        goto bail;
    }

    /* compare the CRC */
    if (!pArchive->valIgnoreCRC && crc != pHeader->mhMasterCRC) {
        if (!Nu_ShouldIgnoreBadCRC(pArchive, nil, kNuErrBadMHCRC)) {
            err = kNuErrBadMHCRC;
            Nu_ReportError(NU_BLOB, err, "Stored MH CRC=0x%04x, calc=0x%04x",
                pHeader->mhMasterCRC, crc);
            goto bail;
        }
    }

    /*
     * Check for an unusual condition.  GS/ShrinkIt appears to update
     * the archive structure in the disk file periodically as it writes,
     * so it's possible to get an apparently complete archive (with
     * correct CRCs in the master and record headers!) that is actually
     * only partially written.  I did this by accident when archiving a
     * 3.5" disk across a slow AppleTalk network.  The only obvious
     * indication of brain-damage, until you try to unpack the archive,
     * seems to be a bogus MasterEOF==48.
     *
     * Matthew Fischer found some archives that exhibit MasterEOF==0
     * but are otherwise functional, suggesting that there might be a
     * version of ShrinkIt that created these without reporting an error.
     * One such archive was a disk image with no filename entry, suggesting
     * that it was created by an early version of P8 ShrinkIt.
     *
     * So, we only fail if the EOF equals 48.
     */
    if (pHeader->mhMasterEOF == kNuMasterHeaderSize) {
        err = kNuErrNoRecords;
        Nu_ReportError(NU_BLOB, err,
            "Master EOF is %ld, archive is probably truncated",
            pHeader->mhMasterEOF);
        goto bail;
    }

    /*
     * Set up a few things in the archive structure on our way out.
     */
    if (isBinary2) {
        if (isSea)
            pArchive->archiveType = kNuArchiveNuFXSelfExInBNY;
        else
            pArchive->archiveType = kNuArchiveNuFXInBNY;
    } else {
        if (isSea)
            pArchive->archiveType = kNuArchiveNuFXSelfEx;
        else
            pArchive->archiveType = kNuArchiveNuFX;
    }

    if (isSea || isBinary2) {
        DBUG(("--- Archive isSea=%d isBinary2=%d type=%d\n",
            isSea, isBinary2, pArchive->archiveType));
    }

    /*pArchive->origNumRecords = pHeader->mhTotalRecords;*/
    pArchive->currentOffset = pArchive->headerOffset + kNuMasterHeaderSize;

    /*DBUG(("--- GOT: records=%ld, vers=%d, EOF=%ld, type=%d, hdrOffset=%ld\n",
        pHeader->mhTotalRecords, pHeader->mhMasterVersion,
        pHeader->mhMasterEOF, pArchive->archiveType, pArchive->headerOffset));*/

    pHeader->isValid = true;

bail:
    return err;
}


/*
 * Prepare the NuArchive and NuMasterHeader structures for use with a
 * newly-created archive.
 */
static void
Nu_InitNewArchive(NuArchive* pArchive)
{
    NuMasterHeader* pHeader;
    
    Assert(pArchive != nil);

    pHeader = &pArchive->masterHeader;

    memcpy(pHeader->mhNufileID, kNuMasterID, kNufileIDLen);
    /*pHeader->mhMasterCRC*/
    pHeader->mhTotalRecords = 0;
    Nu_SetCurrentDateTime(&pHeader->mhArchiveCreateWhen);
    /*pHeader->mhArchiveModWhen*/
    pHeader->mhMasterVersion = kNuOurMHVersion;
    /*pHeader->mhReserved1*/
    pHeader->mhMasterEOF = kNuMasterHeaderSize;
    /*pHeader->mhReserved2*/

    pHeader->isValid = true;

    /* no need to use a temp file for a newly-created archive */
    pArchive->valModifyOrig = true;
}


/*
 * Open an archive in streaming read-only mode.
 */
NuError
Nu_StreamOpenRO(FILE* infp, NuArchive** ppArchive)
{
    NuError err;
    NuArchive* pArchive = nil;

    Assert(infp != nil);
    Assert(ppArchive != nil);

    err = Nu_NuArchiveNew(ppArchive);
    if (err != kNuErrNone)
        goto bail;
    pArchive = *ppArchive;

    pArchive->openMode = kNuOpenStreamingRO;
    pArchive->archiveFp = infp;
    pArchive->archivePathname = strdup("(stream)");

    err = Nu_ReadMasterHeader(pArchive);
    BailError(err);

bail:
    if (err != kNuErrNone) {
        if (pArchive != nil)
            (void) Nu_NuArchiveFree(pArchive);
        *ppArchive = nil;
    }
    return err;
}


/*
 * Open an archive in non-streaming read-only mode.
 */
NuError
Nu_OpenRO(const char* archivePathname, NuArchive** ppArchive)
{
    NuError err;
    NuArchive* pArchive = nil;
    FILE* fp = nil;

    if (archivePathname == nil || !strlen(archivePathname) || ppArchive == nil)
        return kNuErrInvalidArg;

    *ppArchive = nil;

    fp = fopen(archivePathname, kNuFileOpenReadOnly);
    if (fp == nil) {
        Nu_ReportError(NU_BLOB, errno, "Unable to open '%s'", archivePathname);
        err = kNuErrFileOpen;
        goto bail;
    }

    err = Nu_NuArchiveNew(ppArchive);
    if (err != kNuErrNone)
        goto bail;
    pArchive = *ppArchive;

    pArchive->openMode = kNuOpenRO;
    pArchive->archiveFp = fp;
    fp = nil;
    pArchive->archivePathname = strdup(archivePathname);

    err = Nu_ReadMasterHeader(pArchive);
    BailError(err);

bail:
    if (err != kNuErrNone) {
        if (pArchive != nil) {
            (void) Nu_CloseAndFree(pArchive);
            *ppArchive = nil;
        }
        if (fp != nil)
            fclose(fp);
    }
    return err;
}


/*
 * Open a temp file.  If "fileName" contains six Xs ("XXXXXX"), it will
 * be treated as a mktemp-style template, and modified before use.
 *
 * Thought for the day: consider using Win32 SetFileAttributes() to make
 * temp files hidden.  We will need to un-hide it before rolling it over.
 */
static NuError
Nu_OpenTempFile(char* fileName, FILE** pFp)
{
    NuArchive* pArchive = nil;  /* dummy for NU_BLOB */
    NuError err = kNuErrNone;
    int len;

    /*
     * If this is a mktemp-style template, use mktemp or mkstemp to fill in
     * the blanks.
     *
     * BUG: not all implementations of mktemp actually generate a unique
     * name.  We probably need to do probing here.  Some BSD variants like
     * to complain about mktemp, since it's generally a bad way to do
     * things.
     */
    len = strlen(fileName);
    if (len > 6 && strcmp(fileName + len - 6, "XXXXXX") == 0) {
#if defined(HAVE_MKSTEMP) && defined(HAVE_FDOPEN)
        int fd;

        DBUG(("+++ Using mkstemp\n"));

        /* this modifies the template *and* opens the file */
        fd = mkstemp(fileName);
        if (fd < 0) {
            err = errno ? errno : kNuErrFileOpen;
            Nu_ReportError(NU_BLOB, kNuErrNone, "mkstemp failed on '%s'",
                fileName);
            goto bail;
        }

        DBUG(("--- Fd-opening temp file '%s'\n", fileName));
        *pFp = fdopen(fd, kNuFileOpenReadWriteCreat);
        if (*pFp == nil) {
            close(fd);
            err = errno ? errno : kNuErrFileOpen;
            goto bail;
        }

        /* file is open, we're done */
        goto bail;

#else
        char* result;

        DBUG(("+++ Using mktemp\n"));
        result = mktemp(fileName);
        if (result == nil) {
            Nu_ReportError(NU_BLOB, kNuErrNone, "mktemp failed on '%s'",
                fileName);
            err = kNuErrInternal;
            goto bail;
        }

        /* now open the filename as usual */
#endif
    }

    DBUG(("--- Opening temp file '%s'\n", fileName));

#if defined(HAVE_FDOPEN)
    {
        int fd;

        fd = open(fileName, O_RDWR|O_CREAT|O_EXCL|O_BINARY, 0600);
        if (fd < 0) {
            err = errno ? errno : kNuErrFileOpen;
            goto bail;
        }

        *pFp = fdopen(fd, kNuFileOpenReadWriteCreat);
        if (*pFp == nil) {
            close(fd);
            err = errno ? errno : kNuErrFileOpen;
            goto bail;
        }
    }
#else
    /* (not sure how portable "access" is... I think it's POSIX) */
    if (access(fileName, F_OK) == 0) {
        err = kNuErrFileExists;
        goto bail;
    }

    *pFp = fopen(fileName, kNuFileOpenReadWriteCreat);
    if (*pFp == nil) {
        err = errno ? errno : kNuErrFileOpen;
        goto bail;
    }
#endif


bail:
    return err;
}

/*
 * Open an archive in read-write mode, optionally creating it if it doesn't
 * exist.
 */
NuError
Nu_OpenRW(const char* archivePathname, const char* tmpPathname, ulong flags,
    NuArchive** ppArchive)
{
    NuError err;
    FILE* fp = nil;
    FILE* tmpFp = nil;
    NuArchive* pArchive = nil;
    char* tmpPathDup = nil;
    Boolean archiveExists;
    Boolean newlyCreated;

    if (archivePathname == nil || !strlen(archivePathname) ||
        tmpPathname == nil || !strlen(tmpPathname) || ppArchive == nil ||
        (flags & ~(kNuOpenCreat|kNuOpenExcl)) != 0)
    {
        return kNuErrInvalidArg;
    }

    archiveExists = (access(archivePathname, F_OK) == 0);

    /*
     * Open or create archive file.
     */
    if (archiveExists) {
        if ((flags & kNuOpenCreat) && (flags & kNuOpenExcl)) {
            err = kNuErrFileExists;
            Nu_ReportError(NU_BLOB, err, "File '%s' exists", archivePathname);
            goto bail;
        }
        fp = fopen(archivePathname, kNuFileOpenReadWrite);
        newlyCreated = false;
    } else {
        if (!(flags & kNuOpenCreat)) {
            err = kNuErrFileNotFound;
            Nu_ReportError(NU_BLOB, err, "File '%s' not found",archivePathname);
            goto bail;
        }
        fp = fopen(archivePathname, kNuFileOpenReadWriteCreat);
        newlyCreated = true;
    }

    if (fp == nil) {
        if (errno == EACCES)
            err = kNuErrFileAccessDenied;
        else
            err = kNuErrFileOpen;
        Nu_ReportError(NU_BLOB, errno, "Unable to open '%s'", archivePathname);
        goto bail;
    }

    /*
     * Treat zero-length files as newly-created archives.
     */
    if (archiveExists && !newlyCreated) {
        long length;

        err = Nu_GetFileLength(nil, fp, &length);
        BailError(err);

        if (!length) {
            DBUG(("--- treating zero-length file as newly created archive\n"));
            newlyCreated = true;
        }
    }

    /*
     * Create a temp file.  We don't need one for a newly-created archive,
     * at least not right away.  It's possible the caller could add some
     * files, flush the changes, and then want to delete them without
     * closing and reopening the archive.
     *
     * So, create a temp file whether we think we need one or not.  Won't
     * do any harm, and might save us some troubles later.
     */
    tmpPathDup = strdup(tmpPathname);
    BailNil(tmpPathDup);
    err = Nu_OpenTempFile(tmpPathDup, &tmpFp);
    if (err != kNuErrNone) {
        Nu_ReportError(NU_BLOB, err, "Failed opening temp file '%s'",
            tmpPathname);
        goto bail;
    }

    err = Nu_NuArchiveNew(ppArchive);
    if (err != kNuErrNone)
        goto bail;
    pArchive = *ppArchive;

    pArchive->openMode = kNuOpenRW;
    pArchive->newlyCreated = newlyCreated;
    pArchive->archivePathname = strdup(archivePathname);
    pArchive->archiveFp = fp;
    fp = nil;
    pArchive->tmpFp = tmpFp;
    tmpFp = nil;
    pArchive->tmpPathname = tmpPathDup;
    tmpPathDup = nil;

    if (archiveExists && !newlyCreated) {
        err = Nu_ReadMasterHeader(pArchive);
        BailError(err);
    } else {
        Nu_InitNewArchive(pArchive);
    }

bail:
    if (err != kNuErrNone) {
        if (pArchive != nil) {
            (void) Nu_CloseAndFree(pArchive);
            *ppArchive = nil;
        }
        if (fp != nil)
            fclose(fp);
        if (tmpFp != nil)
            fclose(tmpFp);
        if (tmpPathDup != nil)
            Nu_Free(pArchive, tmpPathDup);
    }
    return err;
}


/*
 * ===========================================================================
 *      Update an archive
 * ===========================================================================
 */

/*
 * Write the NuFX master header at the current offset.
 */
NuError
Nu_WriteMasterHeader(NuArchive* pArchive, FILE* fp,
    NuMasterHeader* pHeader)
{
    NuError err;
    long crcOffset;
    ushort crc;

    Assert(pArchive != nil);
    Assert(fp != nil);
    Assert(pHeader != nil);
    Assert(pHeader->isValid);
    Assert(pHeader->mhMasterVersion == kNuOurMHVersion);

    crc = 0;

    Nu_WriteBytes(pArchive, fp, pHeader->mhNufileID, kNufileIDLen);
    err = Nu_FTell(fp, &crcOffset);
    BailError(err);
    Nu_WriteTwo(pArchive, fp, 0);
    Nu_WriteFourC(pArchive, fp, pHeader->mhTotalRecords, &crc);
    Nu_WriteDateTimeC(pArchive, fp, pHeader->mhArchiveCreateWhen, &crc);
    Nu_WriteDateTimeC(pArchive, fp, pHeader->mhArchiveModWhen, &crc);
    Nu_WriteTwoC(pArchive, fp, pHeader->mhMasterVersion, &crc);
    Nu_WriteBytesC(pArchive, fp, pHeader->mhReserved1,
                        kNufileMasterReserved1Len, &crc);
    Nu_WriteFourC(pArchive, fp, pHeader->mhMasterEOF, &crc);
    Nu_WriteBytesC(pArchive, fp, pHeader->mhReserved2,
                        kNufileMasterReserved2Len, &crc);
    
    /* go back and write the CRC (sadly, the seek will flush the stdio buf) */
    pHeader->mhMasterCRC = crc;
    err = Nu_FSeek(fp, crcOffset, SEEK_SET);
    BailError(err);
    Nu_WriteTwo(pArchive, fp, pHeader->mhMasterCRC);

    /* check for errors in any of the above writes */
    if ((err = Nu_HeaderIOFailed(pArchive, fp)) != kNuErrNone) {
        Nu_ReportError(NU_BLOB, err, "Failed writing master header");
        goto bail;
    }

    DBUG(("--- Master header written successfully at %ld (crc=0x%04x)\n",
        crcOffset - kNufileIDLen, crc));

bail:
    return err;
}


/*
 * ===========================================================================
 *      Close an archive
 * ===========================================================================
 */

/*
 * Close all open files, and free the memory associated with the structure.
 *
 * If it's a brand-new archive, and we didn't add anything to it, then we
 * want to remove the stub archive file.
 */
static void
Nu_CloseAndFree(NuArchive* pArchive)
{
    if (pArchive->archiveFp != nil) {
        DBUG(("--- Closing archive\n"));
        fclose(pArchive->archiveFp);
        pArchive->archiveFp = nil;
    }

    if (pArchive->tmpFp != nil) {
        DBUG(("--- Closing and removing temp file\n"));
        fclose(pArchive->tmpFp);
        pArchive->tmpFp = nil;
        Assert(pArchive->tmpPathname != nil);
        if (remove(pArchive->tmpPathname) != 0) {
            Nu_ReportError(NU_BLOB, errno, "Unable to remove temp file '%s'",
                pArchive->tmpPathname);
            /* keep going */
        }
    }

    if (pArchive->newlyCreated && Nu_RecordSet_IsEmpty(&pArchive->origRecordSet))
    {
        DBUG(("--- Newly-created archive unmodified; removing it\n"));
        if (remove(pArchive->archivePathname) != 0) {
            Nu_ReportError(NU_BLOB, errno, "Unable to remove archive file '%s'",
                pArchive->archivePathname);
        }
    }
    
    Nu_NuArchiveFree(pArchive);
}

/*
 * Flush pending changes to the archive, then close it.
 */
NuError
Nu_Close(NuArchive* pArchive)
{
    NuError err = kNuErrNone;
    long flushStatus;

    Assert(pArchive != nil);

    if (!Nu_IsReadOnly(pArchive))
        err = Nu_Flush(pArchive, &flushStatus);
    if (err == kNuErrNone)
        Nu_CloseAndFree(pArchive);
    else {
        DBUG(("--- Close NuFlush status was 0x%4lx\n", flushStatus));
    }

    if (err != kNuErrNone) {
        DBUG(("--- Nu_Close returning error %d\n", err));
    }
    return err;
}


/*
 * ===========================================================================
 *      Delete and replace an archive
 * ===========================================================================
 */

/*
 * Delete the archive file, which should already have been closed.
 */
NuError
Nu_DeleteArchiveFile(NuArchive* pArchive)
{
    Assert(pArchive != nil);
    Assert(pArchive->archiveFp == nil);
    Assert(pArchive->archivePathname != nil);

    return Nu_DeleteFile(pArchive->archivePathname);
}

/*
 * Rename the temp file on top of the original archive.  The temp file
 * should be closed, and the archive file should be deleted.
 */
NuError
Nu_RenameTempToArchive(NuArchive* pArchive)
{
    Assert(pArchive != nil);
    Assert(pArchive->archiveFp == nil);
    Assert(pArchive->tmpFp == nil);
    Assert(pArchive->archivePathname != nil);
    Assert(pArchive->tmpPathname != nil);

    return Nu_RenameFile(pArchive->tmpPathname, pArchive->archivePathname);
}

