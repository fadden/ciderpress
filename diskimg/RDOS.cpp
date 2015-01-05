/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Implementation of DiskFSRDOS class.
 */
#include "StdAfx.h"
#include "DiskImgPriv.h"


/*
 * ===========================================================================
 *      DiskFSRDOS
 * ===========================================================================
 */

const int kSctSize = 256;
const int kCatTrack = 1;
const int kNumCatSectors = 11;      // 0 through 10
const int kDirectoryEntryLen = 32;
const int kNumDirEntryPerSect = (256 / kDirectoryEntryLen); // 8

/*
 * See if this looks like a RDOS volume.
 *
 * There are three variants:
 *  RDOS32 (e.g. ComputerAmbush.nib):
 *      13-sector disk
 *      sector (1,0) starts with "RDOS 2"
 *      sector (1,12) has catalog code, CHAIN in (1,11)
 *      uses "physical" ordering
 *      NOTE: track 0 may be unreadable with RDOS 3.2 NibbleDescr
 *  RDOS33 (e.g. disk #199):
 *      16-sector disk
 *      sector (1,0) starts with "RDOS 3"
 *      sector (1,12) has catalog code
 *      uses "ProDOS" ordering
 *  RDOS3 (e.g. disk #108):
 *      16-sector disk, but only 13 sectors of each track are used
 *      sector (1,0) starts with "RDOS 2"
 *      sector (0,1) has catalog code
 *      uses "physical" orering
 *
 * In all cases:
 *      catalog found on (1,0) through (1,10)
 *
 * The initial value of "pFormatFound" is ignored, because we can reliably
 * detect which variant we're looking at.
 */
static DIError TestImage(DiskImg* pImg, DiskImg::SectorOrder imageOrder,
    DiskImg::FSFormat* pFormatFound)
{
    DIError dierr = kDIErrNone;
    uint8_t sctBuf[kSctSize];


    if (pImg->GetNumSectPerTrack() == 13) {
        /* must be a nibble image; check it for RDOS 3.2 */
        dierr = pImg->ReadTrackSectorSwapped(kCatTrack, 0, sctBuf,
                    imageOrder, DiskImg::kSectorOrderPhysical);
        if (dierr != kDIErrNone)
            goto bail;
    } else if (pImg->GetNumSectPerTrack() == 16) {
        /* could be RDOS3 or RDOS 3.3 */
        dierr = pImg->ReadTrackSectorSwapped(kCatTrack, 0, sctBuf,
                    imageOrder, DiskImg::kSectorOrderPhysical);
        if (dierr != kDIErrNone)
            goto bail;
    } else {
        LOGI(" RDOS neither 13 nor 16 sector, bailing");
        goto bail;
    }

    /* check for RDOS string and correct #of blocks */
    if (!(  sctBuf[0] == 'R'+0x80 &&
            sctBuf[1] == 'D'+0x80 &&
            sctBuf[2] == 'O'+0x80 &&
            sctBuf[3] == 'S'+0x80 &&
            sctBuf[4] == ' '+0x80) ||
        !(sctBuf[25] == 26 || sctBuf[25] == 32))
    {
        LOGI(" RDOS no signature found on (%d,0)", kCatTrack);
        dierr = kDIErrGeneric;
        goto bail;
    }

    /*
     * Guess at the format based on the first catalog entry, which usually
     * begins "RDOS 2.0", "RDOS 2.1", or "RDOS 3.3".
     */
    if (pImg->GetNumSectPerTrack() == 13) {
        *pFormatFound = DiskImg::kFormatRDOS32;
    } else {
        if (sctBuf[5] == '2'+0x80)
            *pFormatFound = DiskImg::kFormatRDOS3;
        else
            *pFormatFound = DiskImg::kFormatRDOS33;
    }

    /*
     * The above came from sector 0, which doesn't help us figure out the
     * sector ordering.  Look for the catalog code.
     */
    {
        int track, sector, offset;
        uint8_t orMask;
        static const char* kCompare = "<NAME>";
        DiskImg::SectorOrder order;

        if (*pFormatFound == DiskImg::kFormatRDOS32 ||
            *pFormatFound == DiskImg::kFormatRDOS3)
        {
            track = 1;
            sector = 12;
            offset = 0xa2;
            orMask = 0x80;
            order = DiskImg::kSectorOrderPhysical;
        } else {
            track = 0;
            sector = 1;
            offset = 0x98;
            orMask = 0;
            order = DiskImg::kSectorOrderProDOS;
        }

        dierr = pImg->ReadTrackSectorSwapped(track, sector, sctBuf,
                    imageOrder, order);
        if (dierr != kDIErrNone)
            goto bail;

        int i;
        for (i = strlen(kCompare)-1; i >= 0; i--) {
            if (sctBuf[offset+i] != ((uint8_t)kCompare[i] | orMask))
                break;
        }
        if (i >= 0) {
            dierr = kDIErrGeneric;
            goto bail;
        }

        LOGI(" RDOS found '%s' signature (order=%d)", kCompare, imageOrder);
    }

    dierr = kDIErrNone;

bail:
    return dierr;
}

/*
 * Common RDOS test code.
 */
/*static*/ DIError DiskFSRDOS::TestFS(DiskImg* pImg, DiskImg::SectorOrder* pOrder,
    DiskImg::FSFormat* pFormat, FSLeniency leniency)
{
    if (!pImg->GetHasSectors()) {
        LOGI(" RDOS - image doesn't have sectors, not trying");
        return kDIErrFilesystemNotFound;
    }
    if (pImg->GetNumTracks() != 35) {
        LOGI(" RDOS - not a 35-track disk, not trying");
        return kDIErrFilesystemNotFound;
    }
    DiskImg::FSFormat formatFound;

    DiskImg::SectorOrder ordering[DiskImg::kSectorOrderMax];
    
    DiskImg::GetSectorOrderArray(ordering, *pOrder);

    for (int i = 0; i < DiskImg::kSectorOrderMax; i++) {
        if (ordering[i] == DiskImg::kSectorOrderUnknown)
            continue;
        if (TestImage(pImg, ordering[i], &formatFound) == kDIErrNone) {
            *pFormat = formatFound;
            *pOrder = ordering[i];
            //*pFormat = DiskImg::kFormatXXX;
            return kDIErrNone;
        }
    }

    LOGI(" RDOS didn't find valid FS");
    return kDIErrFilesystemNotFound;
}

#if 0
/*
 * Test to see if the image is an RDOS 3.3 disk.
 */
/*static*/ DIError DiskFSRDOS::TestFS33(DiskImg* pImg, DiskImg::SectorOrder* pOrder,
    FSLeniency leniency)
{
    DIError dierr;
    DiskImg::FSFormat formatFound = DiskImg::kFormatUnknown;

    dierr = TestCommon(pImg, pOrder, leniency, &formatFound);
    if (dierr != kDIErrNone)
        return dierr;
    if (formatFound != DiskImg::kFormatRDOS33) {
        LOGI(" RDOS found RDOS but wrong type");
        return kDIErrFilesystemNotFound;
    }

    return kDIErrNone;
}

/*
 * Test to see if the image is an RDOS 3.2 disk.
 */
/*static*/ DIError DiskFSRDOS::TestFS32(DiskImg* pImg, DiskImg::SectorOrder* pOrder,
    FSLeniency leniency)
{
    DIError dierr;
    DiskImg::FSFormat formatFound = DiskImg::kFormatUnknown;

    dierr = TestCommon(pImg, pOrder, leniency, &formatFound);
    if (dierr != kDIErrNone)
        return dierr;
    if (formatFound != DiskImg::kFormatRDOS32) {
        LOGI(" RDOS found RDOS but wrong type");
        return kDIErrFilesystemNotFound;
    }

    return kDIErrNone;
}

/*
 * Test to see if the image is an RDOS 3 (cracked 3.2) disk.
 */
/*static*/ DIError DiskFSRDOS::TestFS3(DiskImg* pImg, DiskImg::SectorOrder* pOrder,
    FSLeniency leniency)
{
    DIError dierr;
    DiskImg::FSFormat formatFound = DiskImg::kFormatUnknown;

    dierr = TestCommon(pImg, pOrder, leniency, &formatFound);
    if (dierr != kDIErrNone)
        return dierr;
    if (formatFound != DiskImg::kFormatRDOS3) {
        LOGI(" RDOS found RDOS but wrong type");
        return kDIErrFilesystemNotFound;
    }

    return kDIErrNone;
}
#endif


/*
 * Get things rolling.
 *
 * Since we're assured that this is a valid disk, errors encountered from here
 * on out must be handled somehow, possibly by claiming that the disk is
 * completely full and has no files on it.
 */
DIError DiskFSRDOS::Initialize(void)
{
    DIError dierr = kDIErrNone;
    const char* volStr;

    switch (GetDiskImg()->GetFSFormat()) {
    case DiskImg::kFormatRDOS33:
        volStr = "RDOS 3.3";
        fOurSectPerTrack = 16;
        break;
    case DiskImg::kFormatRDOS32:
        volStr = "RDOS 3.2";
        fOurSectPerTrack = 13;
        break;
    case DiskImg::kFormatRDOS3:
        volStr = "RDOS 3";
        fOurSectPerTrack = 13;
        break;
    default:
        assert(false);
        return kDIErrInternal;
    }
    assert(strlen(volStr) < sizeof(fVolumeName));
    strcpy(fVolumeName, volStr);

    dierr = ReadCatalog();
    if (dierr != kDIErrNone)
        goto bail;

    fVolumeUsage.Create(fpImg->GetNumTracks(), fOurSectPerTrack);
    dierr = ScanFileUsage();
    if (dierr != kDIErrNone) {
        /* this might not be fatal; just means that *some* files are bad */
        goto bail;
    }
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
 * Read the catalog from the disk.
 *
 * To make life easy we slurp the whole thing into memory.
 */
DIError DiskFSRDOS::ReadCatalog(void)
{
    DIError dierr = kDIErrNone;
    uint8_t* dir = NULL;
    uint8_t* dirPtr;
    int track, sector;
    
    dir = new uint8_t[kSctSize * kNumCatSectors];
    if (dir == NULL) {
        dierr = kDIErrMalloc;
        goto bail;
    }

    track = kCatTrack;
    dirPtr = dir;
    for (sector = 0; sector < kNumCatSectors; sector++) {
        dierr = fpImg->ReadTrackSector(track, sector, dirPtr);
        if (dierr != kDIErrNone)
            goto bail;

        dirPtr += kSctSize;
    }

    int i;
    A2FileRDOS* pFile;
    dirPtr = dir;
    for (i = 0; i < kNumCatSectors * kNumDirEntryPerSect;
        i++, dirPtr += kDirectoryEntryLen)
    {
        if (dirPtr[0] == 0x80 || dirPtr[24] == 0xa0)    // deleted file
            continue;
        if (dirPtr[24] == 0x00)     // unused entry; must be at end of catalog
            break;

        pFile = new A2FileRDOS(this);

        memcpy(pFile->fFileName, dirPtr, A2FileRDOS::kMaxFileName);
        pFile->fFileName[A2FileRDOS::kMaxFileName] = '\0';
        pFile->FixFilename();

        switch (dirPtr[24]) {
        case 'A'+0x80:  pFile->fFileType = A2FileRDOS::kTypeApplesoft;  break;
        case 'B'+0x80:  pFile->fFileType = A2FileRDOS::kTypeBinary;     break;
        case 'T'+0x80:  pFile->fFileType = A2FileRDOS::kTypeText;       break;
        // 0x00 is end of catalog, ' '+0x80 is deleted file, both handled above
        default:        pFile->fFileType = A2FileRDOS::kTypeUnknown;    break;
        }
        pFile->fNumSectors = dirPtr[25];
        pFile->fLoadAddr = GetShortLE(&dirPtr[26]);
        pFile->fLength = GetShortLE(&dirPtr[28]);
        pFile->fStartSector = GetShortLE(&dirPtr[30]);

        if (pFile->fStartSector + pFile->fNumSectors >
            fpImg->GetNumTracks() * fOurSectPerTrack)
        {
            LOGI(" RDOS invalid start/count (%d + %d) (max %ld) '%s'",
                pFile->fStartSector, pFile->fNumSectors, fpImg->GetNumBlocks(),
                pFile->fFileName);
            pFile->fStartSector = pFile->fNumSectors = 0;
            pFile->fLength = 0;
            pFile->SetQuality(A2File::kQualityDamaged);
        }

        AddFileToList(pFile);
    }

bail:
    delete[] dir;
    return dierr;
}


/*
 * Create the volume usage map.  Since RDOS volumes have neither
 * in-use maps nor index blocks, this is pretty straightforward.
 */
DIError DiskFSRDOS::ScanFileUsage(void)
{
    int track, sector, block, count;

    A2FileRDOS* pFile;
    pFile = (A2FileRDOS*) GetNextFile(NULL);
    while (pFile != NULL) {
        block = pFile->fStartSector;
        count = pFile->fNumSectors;
        while (count--) {
            track = block / fOurSectPerTrack;
            sector = block % fOurSectPerTrack;

            SetSectorUsage(track, sector, VolumeUsage::kChunkPurposeUserData);

            block++;
        }

        pFile = (A2FileRDOS*) GetNextFile(pFile);
    }

    return kDIErrNone;
}

/*
 * Update an entry in the usage map.
 */
void DiskFSRDOS::SetSectorUsage(long track, long sector,
    VolumeUsage::ChunkPurpose purpose)
{
    VolumeUsage::ChunkState cstate;

    fVolumeUsage.GetChunkState(track, sector, &cstate);
    if (cstate.isUsed) {
        cstate.purpose = VolumeUsage::kChunkPurposeConflict;
        LOGI(" RDOS conflicting uses for sct=(%ld,%ld)", track, sector);
    } else {
        cstate.isUsed = true;
        cstate.isMarkedUsed = true;
        cstate.purpose = purpose;
    }
    fVolumeUsage.SetChunkState(track, sector, &cstate);
}


/*
 * ===========================================================================
 *      A2FileRDOS
 * ===========================================================================
 */

/*
 * Convert RDOS file type to ProDOS file type.
 */
uint32_t A2FileRDOS::GetFileType(void) const
{
    uint32_t retval;

    switch (fFileType) {
    case kTypeText:         retval = 0x04;  break;  // TXT
    case kTypeApplesoft:    retval = 0xfc;  break;  // BAS
    case kTypeBinary:       retval = 0x06;  break;  // BIN
    case kTypeUnknown:
    default:                retval = 0x00;  break;  // NON
    }

    return retval;
}

/*
 * Dump the contents of the A2File structure.
 */
void A2FileRDOS::Dump(void) const
{
    LOGI("A2FileRDOS '%s' (type=%d)", fFileName, fFileType);
    LOGI("  start=%d num=%d len=%d addr=0x%04x",
        fStartSector, fNumSectors, fLength, fLoadAddr);
}

/*
 * "Fix" an RDOS filename.  Convert DOS-ASCII to normal ASCII, and strip
 * trailing spaces.
 *
 * It's possible that RDOS 3.3 forces the filename to high-ASCII, because
 * one disk (#938A) has a file left by the crackers whose name is in
 * low-ASCII.  The inverse-mode correction turns it into punctuation, but
 * I don't see a good way around it.  Or any particular need to fix it.
 */
void A2FileRDOS::FixFilename(void)
{
    DiskFSDOS33::LowerASCII((uint8_t*)fFileName, kMaxFileName);
    TrimTrailingSpaces(fFileName);
}

/*
 * Trim the spaces off the end of a filename.
 *
 * Assumes the filename has already been converted to low ASCII.
 */
void A2FileRDOS::TrimTrailingSpaces(char* filename)
{
    char* lastspc = filename + strlen(filename);

    assert(*lastspc == '\0');

    while (--lastspc) {
        if (*lastspc != ' ')
            break;
    }

    *(lastspc+1) = '\0';
}

/*
 * Not a whole lot to do, since there's no fancy index blocks.
 */
DIError A2FileRDOS::Open(A2FileDescr** ppOpenFile, bool readOnly,
    bool rsrcFork /*=false*/)
{
    if (fpOpenFile != NULL)
        return kDIErrAlreadyOpen;
    if (rsrcFork)
        return kDIErrForkNotFound;
    assert(readOnly == true);

    A2FDRDOS* pOpenFile = new A2FDRDOS(this);

    pOpenFile->fOffset = 0;
    //fOpen = true;

    fpOpenFile = pOpenFile;
    *ppOpenFile = pOpenFile;
    pOpenFile = NULL;

    return kDIErrNone;
}


/*
 * ===========================================================================
 *      A2FDRDOS
 * ===========================================================================
 */

/*
 * Read a chunk of data from the current offset.
 */
DIError A2FDRDOS::Read(void* buf, size_t len, size_t* pActual)
{
    LOGD(" RDOS reading %lu bytes from '%s' (offset=%ld)",
        (unsigned long) len, fpFile->GetPathName(), (long) fOffset);
    //if (!fOpen)
    //  return kDIErrNotReady;

    A2FileRDOS* pFile = (A2FileRDOS*) fpFile;

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
    uint8_t sctBuf[kSctSize];
    long block = pFile->fStartSector + (long) (fOffset / kSctSize);
    int bufOffset = (int) (fOffset % kSctSize);     // (& 0xff)
    int ourSectPerTrack = GetOurSectPerTrack();
    size_t thisCount;

    if (len == 0) {
        ///* one block allocated for empty file */
        //SetLastBlock(block, true);
        return kDIErrNone;
    }
    assert(pFile->fLength != 0);

    while (len) {
        assert(block >= pFile->fStartSector &&
               block < pFile->fStartSector + pFile->fNumSectors);

        dierr = pFile->GetDiskFS()->GetDiskImg()->ReadTrackSector(block / ourSectPerTrack,
                    block % ourSectPerTrack, sctBuf);
        if (dierr != kDIErrNone) {
            LOGI(" RDOS error reading file '%s'", pFile->fFileName);
            return dierr;
        }
        thisCount = kSctSize - bufOffset;
        if (thisCount > len)
            thisCount = len;

        memcpy(buf, sctBuf + bufOffset, thisCount);
        len -= thisCount;
        buf = (char*)buf + thisCount;

        bufOffset = 0;
        block++;
    }

    fOffset += incrLen;

    return dierr;
}

/*
 * Write data at the current offset.
 */
DIError A2FDRDOS::Write(const void* buf, size_t len, size_t* pActual)
{
    //if (!fOpen)
    //  return kDIErrNotReady;
    return kDIErrNotSupported;
}

/*
 * Seek to a new offset.
 */
DIError A2FDRDOS::Seek(di_off_t offset, DIWhence whence)
{
    //if (!fOpen)
    //  return kDIErrNotReady;

    long fileLen = ((A2FileRDOS*) fpFile)->fLength;

    switch (whence) {
    case kSeekSet:
        if (offset < 0 || offset > fileLen)
            return kDIErrInvalidArg;
        fOffset = offset;
        break;
    case kSeekEnd:
        if (offset > 0 || offset < -fileLen)
            return kDIErrInvalidArg;
        fOffset = fileLen + offset;
        break;
    case kSeekCur:
        if (offset < -fOffset ||
            offset >= (fileLen - fOffset))
        {
            return kDIErrInvalidArg;
        }
        fOffset += offset;
        break;
    default:
        assert(false);
        return kDIErrInvalidArg;
    }

    assert(fOffset >= 0 && fOffset <= fileLen);
    return kDIErrNone;
}

/*
 * Return current offset.
 */
di_off_t A2FDRDOS::Tell(void)
{
    //if (!fOpen)
    //  return kDIErrNotReady;

    return fOffset;
}

/*
 * Release file state, such as it is.
 */
DIError A2FDRDOS::Close(void)
{
    fpFile->CloseDescr(this);
    return kDIErrNone;
}

/*
 * Return the #of sectors/blocks in the file.
 */
long A2FDRDOS::GetSectorCount(void) const
{
    //if (!fOpen)
    //  return kDIErrNotReady;
    return ((A2FileRDOS*) fpFile)->fNumSectors;
}

long A2FDRDOS::GetBlockCount(void) const
{
    //if (!fOpen)
    //  return kDIErrNotReady;
    return ((A2FileRDOS*) fpFile)->fNumSectors / 2;
}

/*
 * Return the Nth track/sector in this file.
 */
DIError A2FDRDOS::GetStorage(long sectorIdx, long* pTrack, long* pSector) const
{
    //if (!fOpen)
    //  return kDIErrNotReady;
    A2FileRDOS* pFile = (A2FileRDOS*) fpFile;
    long rdosBlock = pFile->fStartSector + sectorIdx;
    int ourSectPerTrack = GetOurSectPerTrack();
    if (rdosBlock >= pFile->fStartSector + pFile->fNumSectors)
        return kDIErrInvalidIndex;

    *pTrack = rdosBlock / ourSectPerTrack;
    *pSector = rdosBlock % ourSectPerTrack;

    return kDIErrNone;
}

/*
 * Return the Nth 512-byte block in this file.
 */
DIError A2FDRDOS::GetStorage(long blockIdx, long* pBlock) const
{
    //if (!fOpen)
    //  return kDIErrNotReady;
    A2FileRDOS* pFile = (A2FileRDOS*) fpFile;
    long rdosBlock = pFile->fStartSector + blockIdx*2;
    if (rdosBlock >= pFile->fStartSector + pFile->fNumSectors)
        return kDIErrInvalidIndex;

    *pBlock = rdosBlock / 2;

    if (pFile->GetDiskFS()->GetDiskImg()->GetHasBlocks()) {
        assert(*pBlock < pFile->GetDiskFS()->GetDiskImg()->GetNumBlocks());
    }
    return kDIErrNone;
}
