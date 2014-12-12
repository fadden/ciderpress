/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Apple II CP/M disk format.
 *
 * Limitations:
 *  - Read-only.
 *  - Does not do much with user numbers.
 *  - Rumor has it that "sparse" files are possible.  Not handled.
 *  - I'm currently treating the directory as fixed-length.  This may
 *    not be correct.
 *  - Not handling special entries (volume label, date stamps,
 *    password control).
 *
 * As I have no practical experience with CP/M, this is the weakest of the
 * filesystem implementations.
 */
#include "StdAfx.h"
#include "DiskImgPriv.h"


/*
 * ===========================================================================
 *      DiskFSCPM
 * ===========================================================================
 */

const int kBlkSize = 512;               // really ought to be 1024
const int kVolDirBlock = 24;            // track 3 sector 0
const int kVolDirCount = 4;             // 4 prodos blocks
const int kNoDataByte = 0xe5;
const int kMaxUserNumber = 31;          // 0-15 on some systems, 0-31 on others
const int kMaxSpecialUserNumber = 0x21; // 0x20 and 0x21 have special meanings
const int kMaxExtent = 31;              // extent counter, 0-31

/*
 * See if this looks like a CP/M volume.
 *
 * We test a few fields in the volume directory for validity.
 */
static DIError TestImage(DiskImg* pImg, DiskImg::SectorOrder imageOrder)
{
    DIError dierr = kDIErrNone;
    uint8_t dirBuf[kBlkSize * kVolDirCount];
    uint8_t* dptr;
    int i;

    assert(sizeof(dirBuf) == DiskFSCPM::kFullDirSize);

    for (i = 0; i < kVolDirCount; i++) {
        dierr = pImg->ReadBlockSwapped(kVolDirBlock + i, dirBuf + kBlkSize*i,
                    imageOrder, DiskImg::kSectorOrderCPM);
        if (dierr != kDIErrNone)
            goto bail;
    }

    dptr = dirBuf;
    for (i = 0; i < DiskFSCPM::kFullDirSize/DiskFSCPM::kDirectoryEntryLen; i++)
    {
        if (*dptr != kNoDataByte) {
            /*
             * Usually userNumber is 0, but sometimes not.  It's expected to
             * be < 0x20 for a normal file, may be 0x21 or 0x22 for special
             * entries (volume label, date stamps).
             */
            if (*dptr > kMaxSpecialUserNumber) {
                dierr = kDIErrFilesystemNotFound;
                break;
            }

            /* extent counter, 0-31 */
            if (dptr[12] > kMaxExtent) {
                dierr = kDIErrFilesystemNotFound;
                break;
            }

            /* check for a valid filename here; high bit may be set on some bytes */
            uint8_t firstLet = *(dptr+1) & 0x7f;
            if (firstLet < 0x20) {
                dierr = kDIErrFilesystemNotFound;
                break;
            }
        }
        dptr += DiskFSCPM::kDirectoryEntryLen;
    }
    if (dierr == kDIErrNone) {
        LOGI(" CPM found clean directory, imageOrder=%d", imageOrder);
    }

bail:
    return dierr;
}

/*
 * Test to see if the image is a CP/M disk.
 *
 * On the Apple II, these were always on 5.25" disks.  However, it's possible
 * to create hard drive volumes up to 8MB.
 */
/*static*/ DIError DiskFSCPM::TestFS(DiskImg* pImg, DiskImg::SectorOrder* pOrder,
    DiskImg::FSFormat* pFormat, FSLeniency leniency)
{
    /* CP/M disks use 1K blocks, so ignore anything with odd count */
    if (pImg->GetNumBlocks() == 0 ||
        (pImg->GetNumBlocks() & 0x01) != 0)
    {
        LOGI(" CPM rejecting image with numBlocks=%ld",
            pImg->GetNumBlocks());
        return kDIErrFilesystemNotFound;
    }

    DiskImg::SectorOrder ordering[DiskImg::kSectorOrderMax];
    
    DiskImg::GetSectorOrderArray(ordering, *pOrder);

    for (int i = 0; i < DiskImg::kSectorOrderMax; i++) {
        if (ordering[i] == DiskImg::kSectorOrderUnknown)
            continue;
        if (TestImage(pImg, ordering[i]) == kDIErrNone) {
            *pOrder = ordering[i];
            *pFormat = DiskImg::kFormatCPM;
            return kDIErrNone;
        }
    }

    LOGI(" CPM didn't find valid FS");
    return kDIErrFilesystemNotFound;
}

/*
 * Get things rolling.
 *
 * Since we're assured that this is a valid disk, errors encountered from here
 * on out must be handled somehow, possibly by claiming that the disk is
 * completely full and has no files on it.
 */
DIError DiskFSCPM::Initialize(void)
{
    DIError dierr = kDIErrNone;

    dierr = ReadCatalog();
    if (dierr != kDIErrNone)
        goto bail;

    fVolumeUsage.Create(fpImg->GetNumBlocks());
    dierr = ScanFileUsage();
    if (dierr != kDIErrNone) {
        /* this might not be fatal; just means that *some* files are bad */
        dierr = kDIErrNone;
        goto bail;
    }

    fDiskIsGood = CheckDiskIsGood();

    fVolumeUsage.Dump();

    //A2File* pFile;
    //pFile = GetNextFile(NULL);
    //while (pFile != NULL) {
    //  pFile->Dump();
    //  pFile = GetNextFile(pFile);
    //}

bail:
    return dierr;
}

/*
 * Read the entire CP/M catalog (all 2K of it) into memory, and parse
 * out the individual files.
 *
 * A single file can have more than one directory entry.  We only want
 * to create an A2File object for the first one.
 */
DIError DiskFSCPM::ReadCatalog(void)
{
    DIError dierr = kDIErrNone;
    uint8_t dirBuf[kFullDirSize];
    uint8_t* dptr;
    int i;

    for (i = 0; i < kVolDirCount; i++) {
        dierr = fpImg->ReadBlock(kVolDirBlock + i, dirBuf + kBlkSize*i);
        if (dierr != kDIErrNone)
            goto bail;
    }

    dptr = dirBuf;
    for (i = 0; i < kNumDirEntries; i++) {
        fDirEntry[i].userNumber = dptr[0x00];
        /* copy the filename, stripping the high bits off */
        for (int j = 0; j < kDirFileNameLen; j++)
            fDirEntry[i].fileName[j] = dptr[0x01 + j] & 0x7f;
        fDirEntry[i].fileName[kDirFileNameLen] = '\0';
        fDirEntry[i].extent = dptr[0x0c] + dptr[0x0e] * kExtentsInLowByte;
        fDirEntry[i].S1 = dptr[0x0d];
        fDirEntry[i].records = dptr[0x0f];
        memcpy(fDirEntry[i].blocks, &dptr[0x10], kDirEntryBlockCount);
        fDirEntry[i].readOnly = (dptr[0x09] & 0x80) != 0;
        fDirEntry[i].system = (dptr[0x0a] & 0x80) != 0;
        fDirEntry[i].badBlockList = false;  // set if block list is bad

        dptr += kDirectoryEntryLen;
    }

    /* create an entry for the first extent of each file */
    for (i = 0; i < kNumDirEntries; i++) {
        A2FileCPM* pFile;

        if (fDirEntry[i].userNumber == kNoDataByte || fDirEntry[i].extent != 0)
            continue;
        if (fDirEntry[i].userNumber > kMaxUserNumber) {
            /* skip over volume label, date stamps, etc */
            LOGI("Skipping entry with userNumber=0x%02x",
                fDirEntry[i].userNumber);
        }

        pFile = new A2FileCPM(this, fDirEntry);
        FormatName(pFile->fFileName, (char*)fDirEntry[i].fileName);
        pFile->fReadOnly = fDirEntry[i].readOnly;
        pFile->fDirIdx = i;

        pFile->fLength = 0;
        dierr = ComputeLength(pFile);
        if (dierr != kDIErrNone) {
            pFile->SetQuality(A2File::kQualityDamaged);
            dierr = kDIErrNone;
        }
        AddFileToList(pFile);
    }

    /*
     * Validate the list of blocks.
     */
    int maxCpmBlock;
    maxCpmBlock = (fpImg->GetNumBlocks() - kVolDirBlock) / 2;
    for (i = 0; i < kNumDirEntries; i++) {
        if (fDirEntry[i].userNumber == kNoDataByte)
            continue;
        for (int j = 0; j < kDirEntryBlockCount; j++) {
            if (fDirEntry[i].blocks[j] >= maxCpmBlock) {
                LOGI(" CPM invalid block %d in file '%s'",
                    fDirEntry[i].blocks[j], fDirEntry[i].fileName);
                //pFile->SetQuality(A2File::kQualityDamaged);
                fDirEntry[i].badBlockList = true;
                break;
            }
        }
    }

bail:
    return dierr;
}

/*
 * Reformat from 11 chars with spaces into clean xxxxx.yyy format.
 */
void DiskFSCPM::FormatName(char* dstBuf, const char* srcBuf)
{
    char workBuf[kDirFileNameLen+1];
    char* cp;

    assert(strlen(srcBuf) < sizeof(workBuf));
    strcpy(workBuf, srcBuf);

    cp = workBuf;
    while (*cp != '\0') {
        //*cp &= 0x7f;      // [no longer necessary]
        if (*cp == ' ')
            *cp = '\0';
        if (*cp == ':')     // don't think this is allowed, but check
            *cp = 'X';      //  for it anyway
        cp++;
    }

    strcpy(dstBuf, workBuf);
    dstBuf[8] = '\0';       // in case filename part is full 8 chars
    strcat(dstBuf, ".");
    strcat(dstBuf, workBuf+8);

    assert(strlen(dstBuf) <= A2FileCPM::kMaxFileName);
}

/*
 * Compute the length of a file.  Sets "pFile->fLength".
 *
 * This requires walking through the list of extents and looking for the
 * last one.  We use the "records" field of the last extent to determine
 * the file length.
 *
 * (Should probably just get the block list and then walk that, rather than
 * having directory parse code in two places.)
 */
DIError DiskFSCPM::ComputeLength(A2FileCPM* pFile)
{
    int i;
    int best, maxExtent;

    best = maxExtent = -1;

    for (i = 0; i < DiskFSCPM::kNumDirEntries; i++) {
        if (fDirEntry[i].userNumber == kNoDataByte)
            continue;

        if (strcmp((const char*)fDirEntry[i].fileName,
                   (const char*)fDirEntry[pFile->fDirIdx].fileName) == 0 &&
            fDirEntry[i].userNumber == fDirEntry[pFile->fDirIdx].userNumber)
        {
            /* this entry is part of the file */
            if (fDirEntry[i].extent > maxExtent) {
                best = i;
                maxExtent = fDirEntry[i].extent;
            }
        }
    }

    if (maxExtent < 0 || best < 0) {
        LOGI("  CPM couldn't find existing file '%s'!", pFile->fFileName);
        assert(false);
        return kDIErrInternal;
    }

    pFile->fLength = kDirEntryBlockCount * 1024 * maxExtent +
                        fDirEntry[best].records * 128;

    return kDIErrNone;
}


/*
 * Scan file usage into the volume usage map.
 *
 * Tracks 0, 1, and 2 are always used by the boot loader.  The volume directory
 * is on the first half of track 3 (blocks 0 and 1).
 */
DIError DiskFSCPM::ScanFileUsage(void)
{
    int cpmBlock;
    int i, j;

    for (i = 0; i < kVolDirBlock; i++)
        SetBlockUsage(i, VolumeUsage::kChunkPurposeSystem);
    for (i = kVolDirBlock; i < kVolDirBlock + kVolDirCount; i++)
        SetBlockUsage(i, VolumeUsage::kChunkPurposeVolumeDir);

    for (i = 0; i < kNumDirEntries; i++) {
        if (fDirEntry[i].userNumber == kNoDataByte)
            continue;
        if (fDirEntry[i].badBlockList)
            continue;

        for (j = 0; j < kDirEntryBlockCount; j++) {
            cpmBlock = fDirEntry[i].blocks[j];
            if (cpmBlock == 0)
                break;
            SetBlockUsage(CPMToProDOSBlock(cpmBlock),
                VolumeUsage::kChunkPurposeUserData);
            SetBlockUsage(CPMToProDOSBlock(cpmBlock)+1,
                VolumeUsage::kChunkPurposeUserData);
        }
    }

    return kDIErrNone;
}

/*
 * Update an entry in the usage map.
 *
 * "block" is a 512-byte block, so you will have to call here twice for every
 * 1K CP/M block.
 */
void DiskFSCPM::SetBlockUsage(long block, VolumeUsage::ChunkPurpose purpose)
{
    VolumeUsage::ChunkState cstate;

    if (fVolumeUsage.GetChunkState(block, &cstate) != kDIErrNone) {
        LOGI(" CPM ERROR: unable to set state on block %ld", block);
        return;
    }

    if (cstate.isUsed) {
        cstate.purpose = VolumeUsage::kChunkPurposeConflict;
        LOGI(" CPM conflicting uses for block=%ld", block);
    } else {
        cstate.isUsed = true;
        cstate.isMarkedUsed = true;     // no volume bitmap
        cstate.purpose = purpose;
    }
    fVolumeUsage.SetChunkState(block, &cstate);
}


/*
 * Scan for damaged files and conflicting file allocation entries.
 *
 * Appends some entries to the DiskImg notes, so this should only be run
 * once per DiskFS.
 *
 * Returns "true" if disk appears to be perfect, "false" otherwise.
 */
bool DiskFSCPM::CheckDiskIsGood(void)
{
    //DIError dierr;
    bool result = true;

    //if (fEarlyDamage)
    //  result = false;

    /*
     * TO DO: look for multiple files occupying the same blocks.
     */

    /*
     * Scan for "damaged" or "suspicious" files diagnosed earlier.
     */
    bool damaged, suspicious;
    ScanForDamagedFiles(&damaged, &suspicious);

    if (damaged) {
        fpImg->AddNote(DiskImg::kNoteWarning,
            "One or more files are damaged.");
        result = false;
    } else if (suspicious) {
        fpImg->AddNote(DiskImg::kNoteWarning,
            "One or more files look suspicious.");
        result = false;
    }

    return result;
}


/*
 * ===========================================================================
 *      A2FileCPM
 * ===========================================================================
 */

/*
 * Not a whole lot to do, since there's no fancy index blocks.
 *
 * Calling GetBlockList twice is probably not the best way to go through life.
 * This needs an overhaul.
 */
DIError A2FileCPM::Open(A2FileDescr** ppOpenFile, bool readOnly,
    bool rsrcFork /*=false*/)
{
    DIError dierr;
    A2FDCPM* pOpenFile = NULL;

    if (fpOpenFile != NULL)
        return kDIErrAlreadyOpen;
    if (rsrcFork)
        return kDIErrForkNotFound;

    assert(readOnly);

    pOpenFile = new A2FDCPM(this);

    dierr = GetBlockList(&pOpenFile->fBlockCount, NULL);
    if (dierr != kDIErrNone)
        goto bail;

    pOpenFile->fBlockList = new uint8_t[pOpenFile->fBlockCount+1];
    pOpenFile->fBlockList[pOpenFile->fBlockCount] = 0xff;

    dierr = GetBlockList(&pOpenFile->fBlockCount, pOpenFile->fBlockList);
    if (dierr != kDIErrNone)
        goto bail;

    assert(pOpenFile->fBlockList[pOpenFile->fBlockCount] == 0xff);

    pOpenFile->fOffset = 0;
    //fOpen = true;

    fpOpenFile = pOpenFile;
    *ppOpenFile = pOpenFile;
    pOpenFile = NULL;

bail:
    delete pOpenFile;
    return dierr;
}


/*
 * Get the complete block list for a file.  This will involve reading
 * one or more directory entries.
 *
 * Call this once with "blockBuf" equal to "NULL" to get the block count,
 * then call a second time after allocating blockBuf.
 */
DIError A2FileCPM::GetBlockList(long* pBlockCount, uint8_t* blockBuf) const
{
    di_off_t length = fLength;
    int blockCount = 0;
    int i, j;

    /*
     * Run through the entries, pulling blocks out until we account for the
     * entire length of the file.
     *
     * [Should probably pay more attention to extent numbers, making sure
     * that they make sense.  Not vital until we allow writes.]
     */
    for (i = 0; i < DiskFSCPM::kNumDirEntries; i++) {
        if (length <= 0)
            break;
        if (fpDirEntry[i].userNumber == kNoDataByte)
            continue;

        if (strcmp((const char*)fpDirEntry[i].fileName,
                   (const char*)fpDirEntry[fDirIdx].fileName) == 0 &&
            fpDirEntry[i].userNumber == fpDirEntry[fDirIdx].userNumber)
        {
            /* this entry is part of the file */
            for (j = 0; j < DiskFSCPM::kDirEntryBlockCount; j++) {
                if (fpDirEntry[i].blocks[j] == 0) {
                    LOGI(" CPM found sparse block %d/%d", i, j);
                }
                blockCount++;

                if (blockBuf != NULL) {
                    long listOffset = j +
                        fpDirEntry[i].extent * DiskFSCPM::kDirEntryBlockCount;
                    blockBuf[listOffset] = fpDirEntry[i].blocks[j];
                }

                length -= 1024;
                if (length <= 0)
                    break;
            }
        }
    }

    if (length > 0) {
        LOGI(" CPM WARNING: can't account for %ld bytes!", (long) length);
        //assert(false);
    }

    //LOGI(" Returning blockCount=%d for '%s'", blockCount,
    //  fpDirEntry[fDirIdx].fileName);
    if (pBlockCount != NULL) {
        assert(blockBuf == NULL || *pBlockCount == blockCount);
        *pBlockCount = blockCount;
    }

    return kDIErrNone;
}

/*
 * Dump the contents of the A2File structure.
 */
void A2FileCPM::Dump(void) const
{
    LOGI("A2FileCPM '%s' length=%ld", fFileName, (long) fLength);
}


/*
 * ===========================================================================
 *      A2FDCPM
 * ===========================================================================
 */

/*
 * Read a chunk of data from the current offset.
 */
DIError A2FDCPM::Read(void* buf, size_t len, size_t* pActual)
{
    LOGI(" CP/M reading %lu bytes from '%s' (offset=%ld)",
        (unsigned long) len, fpFile->GetPathName(), (long) fOffset);

    A2FileCPM* pFile = (A2FileCPM*) fpFile;

    /* don't allow them to read past the end of the file */
    if (fOffset + (long)len > pFile->fLength) {
        if (pActual == NULL)
            return kDIErrDataUnderrun;
        len = (size_t) (pFile->fLength - fOffset);
    }
    if (pActual != NULL)
        *pActual = len;
    long incrLen = len;

    DIError dierr = kDIErrNone;
    const int kCPMBlockSize = kBlkSize*2;
    assert(kCPMBlockSize == 1024);
    uint8_t blkBuf[kCPMBlockSize];
    int blkIndex = (int) (fOffset / kCPMBlockSize);
    int bufOffset = (int) (fOffset % kCPMBlockSize);        // (& 0x3ff)
    size_t thisCount;
    long prodosBlock;

    if (len == 0)
        return kDIErrNone;
    assert(pFile->fLength != 0);

    while (len) {
        if (blkIndex >= fBlockCount) {
            /* ran out of data */
            return kDIErrDataUnderrun;
        }

        if (fBlockList[blkIndex] == 0) {
            /*
             * Sparse block.
             */
            memset(blkBuf, kNoDataByte, sizeof(blkBuf));
        } else {
            /*
             * Read one CP/M block (two ProDOS blocks) and pull out the
             * set of data that the user wants.
             *
             * On some Microsoft Softcard disks, the first three tracks hold
             * file data rather than the system image.
             */
            prodosBlock = DiskFSCPM::CPMToProDOSBlock(fBlockList[blkIndex]);
            if (prodosBlock >= 280)
                prodosBlock -= 280;

            dierr = fpFile->GetDiskFS()->GetDiskImg()->ReadBlock(prodosBlock,
                        blkBuf);
            if (dierr != kDIErrNone) {
                LOGI(" CP/M error1 reading file '%s'", pFile->fFileName);
                return dierr;
            }
            dierr = fpFile->GetDiskFS()->GetDiskImg()->ReadBlock(prodosBlock+1,
                        blkBuf + kBlkSize);
            if (dierr != kDIErrNone) {
                LOGI(" CP/M error2 reading file '%s'", pFile->fFileName);
                return dierr;
            }
        }

        thisCount = kCPMBlockSize - bufOffset;
        if (thisCount > len)
            thisCount = len;

        memcpy(buf, blkBuf + bufOffset, thisCount);
        len -= thisCount;
        buf = (char*)buf + thisCount;

        bufOffset = 0;
        blkIndex++;
    }

    fOffset += incrLen;

    return dierr;
}

/*
 * Write data at the current offset.
 */
DIError A2FDCPM::Write(const void* buf, size_t len, size_t* pActual)
{
    return kDIErrNotSupported;
}

/*
 * Seek to a new offset.
 */
DIError A2FDCPM::Seek(di_off_t offset, DIWhence whence)
{
    di_off_t fileLength = ((A2FileCPM*) fpFile)->fLength;

    switch (whence) {
    case kSeekSet:
        if (offset < 0 || offset > fileLength)
            return kDIErrInvalidArg;
        fOffset = offset;
        break;
    case kSeekEnd:
        if (offset > 0 || offset < -fileLength)
            return kDIErrInvalidArg;
        fOffset = fileLength + offset;
        break;
    case kSeekCur:
        if (offset < -fOffset ||
            offset >= (fileLength - fOffset))
        {
            return kDIErrInvalidArg;
        }
        fOffset += offset;
        break;
    default:
        assert(false);
        return kDIErrInvalidArg;
    }

    assert(fOffset >= 0 && fOffset <= fileLength);
    return kDIErrNone;
}

/*
 * Return current offset.
 */
di_off_t A2FDCPM::Tell(void)
{
    return fOffset;
}

/*
 * Release file state, such as it is.
 */
DIError A2FDCPM::Close(void)
{
    fpFile->CloseDescr(this);
    return kDIErrNone;
}

/*
 * Return the #of sectors/blocks in the file.
 */
long A2FDCPM::GetSectorCount(void) const
{
    return fBlockCount * 4;
}
long A2FDCPM::GetBlockCount(void) const
{
    return fBlockCount * 2;
}

/*
 * Return the Nth track/sector in this file.
 */
DIError A2FDCPM::GetStorage(long sectorIdx, long* pTrack, long* pSector) const
{
    long cpmIdx = sectorIdx / 4;    // 4 256-byte sectors per 1K CP/M block
    if (cpmIdx >= fBlockCount)
        return kDIErrInvalidIndex;  // CP/M files can have *no* storage

    long cpmBlock = fBlockList[cpmIdx];
    long prodosBlock = DiskFSCPM::CPMToProDOSBlock(cpmBlock);
    if (sectorIdx & 0x02)
        prodosBlock++;

    BlockToTrackSector(prodosBlock, (sectorIdx & 0x01) != 0, pTrack, pSector);
    return kDIErrNone;
}
/*
 * Return the Nth 512-byte block in this file.  Since things aren't stored
 * in 512-byte blocks, we grab the appropriate 1K block and pick half.
 */
DIError A2FDCPM::GetStorage(long blockIdx, long* pBlock) const
{
    long cpmIdx = blockIdx / 2; // 4 256-byte sectors per 1K CP/M block
    if (cpmIdx >= fBlockCount)
        return kDIErrInvalidIndex;

    long cpmBlock = fBlockList[cpmIdx];
    long prodosBlock = DiskFSCPM::CPMToProDOSBlock(cpmBlock);
    if (blockIdx & 0x01)
        prodosBlock++;

    *pBlock = prodosBlock;
    assert(*pBlock < fpFile->GetDiskFS()->GetDiskImg()->GetNumBlocks());
    return kDIErrNone;
}
