/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Implementation of DiskFSPascal class.
 *
 * Currently each file may only be open once.
 */
#include "StdAfx.h"
#include "DiskImgPriv.h"


/*
 * ===========================================================================
 *      DiskFSPascal
 * ===========================================================================
 */

const int kBlkSize = 512;
const int kVolHeaderBlock = 2;          // first directory block
const int kMaxCatalogIterations = 64;   // should be short, linear catalog
const int kHugeDir = 32;
static const char* kInvalidNameChars = "$=?,[#:";


/*
 * See if this looks like a Pascal volume.
 *
 * We test a few fields in the volume directory for validity.
 */
static DIError TestImage(DiskImg* pImg, DiskImg::SectorOrder imageOrder)
{
    DIError dierr = kDIErrNone;
    uint8_t blkBuf[512];
    uint8_t volName[DiskFSPascal::kMaxVolumeName+1];

    dierr = pImg->ReadBlockSwapped(kVolHeaderBlock, blkBuf, imageOrder,
                DiskImg::kSectorOrderProDOS);
    if (dierr != kDIErrNone)
        goto bail;


    if (!(blkBuf[0x00] == 0 && blkBuf[0x01] == 0) ||
        !(blkBuf[0x04] == 0 && blkBuf[0x05] == 0) ||
        !(blkBuf[0x06] > 0 && blkBuf[0x06] <= DiskFSPascal::kMaxVolumeName) ||
        0)
    {
        dierr = kDIErrFilesystemNotFound;
        goto bail;
    }

    /* volume name length is good, check the name itself */
    /* (this may be overly restrictive, but it's probably good to be) */
    memset(volName, 0, sizeof(volName));
    memcpy(volName, &blkBuf[0x07], blkBuf[0x06]);
    if (!DiskFSPascal::IsValidVolumeName((const char*) volName))
        return kDIErrFilesystemNotFound;

bail:
    return dierr;
}

/*
 * Test to see if the image is a Pascal disk.
 */
/*static*/ DIError DiskFSPascal::TestFS(DiskImg* pImg, DiskImg::SectorOrder* pOrder,
    DiskImg::FSFormat* pFormat, FSLeniency leniency)
{
    DiskImg::SectorOrder ordering[DiskImg::kSectorOrderMax];
    
    DiskImg::GetSectorOrderArray(ordering, *pOrder);

    for (int i = 0; i < DiskImg::kSectorOrderMax; i++) {
        if (ordering[i] == DiskImg::kSectorOrderUnknown)
            continue;
        if (TestImage(pImg, ordering[i]) == kDIErrNone) {
            *pOrder = ordering[i];
            *pFormat = DiskImg::kFormatPascal;
            return kDIErrNone;
        }
    }

    LOGI(" Pascal didn't find valid FS");
    return kDIErrFilesystemNotFound;
}

/*
 * Get things rolling.
 *
 * Since we're assured that this is a valid disk, errors encountered from here
 * on out must be handled somehow, possibly by claiming that the disk is
 * completely full and has no files on it.
 */
DIError DiskFSPascal::Initialize(void)
{
    DIError dierr = kDIErrNone;

    fDiskIsGood = false;        // hosed until proven innocent
    fEarlyDamage = false;

    fVolumeUsage.Create(fpImg->GetNumBlocks());

    dierr = LoadVolHeader();
    if (dierr != kDIErrNone)
        goto bail;
    DumpVolHeader();

    dierr = ProcessCatalog();
    if (dierr != kDIErrNone)
        goto bail;

    dierr = ScanFileUsage();
    if (dierr != kDIErrNone) {
        /* this might not be fatal; just means that *some* files are bad */
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
 * Read some interesting fields from the volume header.
 */
DIError DiskFSPascal::LoadVolHeader(void)
{
    DIError dierr = kDIErrNone;
    uint8_t blkBuf[kBlkSize];
    int nameLen, maxFiles;

    dierr = fpImg->ReadBlock(kVolHeaderBlock, blkBuf);
    if (dierr != kDIErrNone)
        goto bail;

    /* vol header is same size as dir entry, but different layout */
    fStartBlock = GetShortLE(&blkBuf[0x00]);
    assert(fStartBlock == 0);       // verified in "TestImage"
    fNextBlock = GetShortLE(&blkBuf[0x02]);
    assert(GetShortLE(&blkBuf[0x04]) == 0);     // type
    nameLen = blkBuf[0x06] & 0x07;
    memcpy(fVolumeName, &blkBuf[0x07], nameLen);
    fVolumeName[nameLen] = '\0';
    fTotalBlocks = GetShortLE(&blkBuf[0x0e]);
    fNumFiles = GetShortLE(&blkBuf[0x10]);
    fAccessWhen = GetShortLE(&blkBuf[0x12]);    // time of last access
    fDateSetWhen = GetShortLE(&blkBuf[0x14]);   // most recent date set
    fStuff1 = GetShortLE(&blkBuf[0x16]);        // filler
    fStuff2 = GetShortLE(&blkBuf[0x18]);        // filler

    if (fTotalBlocks != fpImg->GetNumBlocks()) {
        // saw this most recently on a 40-track .APP image; not a problem
        LOGI(" Pascal WARNING: total (%u) != img (%ld)",
            fTotalBlocks, fpImg->GetNumBlocks());
    }

    /* 
     * Sanity checks.
     */
    if (fNextBlock > 34) {
        // directory really shouldn't be more than 6; I'm being generous
        fpImg->AddNote(DiskImg::kNoteWarning,
            "Pascal directory is too big (%d blocks); trimming.",
            fNextBlock - fStartBlock);
    }

    /* max #of file entries, including the vol dir header */
    maxFiles = ((fNextBlock - kVolHeaderBlock) * kBlkSize) / kDirectoryEntryLen;
    if (fNumFiles > maxFiles-1) {
        fpImg->AddNote(DiskImg::kNoteWarning,
            "Pascal fNumFiles (%d) exceeds max files (%d); trimming.\n",
            fNumFiles, maxFiles-1);
        fEarlyDamage = true;
    }

    SetVolumeID();

bail:
    return dierr;
}

/*
 * Set the volume ID field.
 */
void DiskFSPascal::SetVolumeID(void)
{
    sprintf(fVolumeID, "Pascal %s:", fVolumeName);
}

/*
 * Dump what we pulled out of the volume header.
 */
void DiskFSPascal::DumpVolHeader(void)
{
    time_t access, dateSet;

    LOGI(" Pascal volume header for '%s'", fVolumeName);
    LOGI("   startBlock=%d nextBlock=%d",
        fStartBlock, fNextBlock);
    LOGI("   totalBlocks=%d numFiles=%d access=0x%04x dateSet=0x%04x",
        fTotalBlocks, fNumFiles, fAccessWhen, fDateSetWhen);

    access = A2FilePascal::ConvertPascalDate(fAccessWhen);
    dateSet = A2FilePascal::ConvertPascalDate(fDateSetWhen);
    LOGI("   -->access %.24s", ctime(&access));
    LOGI("   -->dateSet %.24s", ctime(&dateSet));

    //LOGI("Unconvert access=0x%04x dateSet=0x%04x",
    //  A2FilePascal::ConvertPascalDate(access),
    //  A2FilePascal::ConvertPascalDate(dateSet));
}


/*
 * Read the catalog from the disk.
 *
 * No distinction is made for block boundaries, so we want to slurp the
 * entire thing into memory.
 *
 * Sets "fDirectory".
 */
DIError DiskFSPascal::LoadCatalog(void)
{
    DIError dierr = kDIErrNone;
    uint8_t* dirPtr;
    int block, numBlocks;

    assert(fDirectory == NULL);

    numBlocks = fNextBlock - kVolHeaderBlock;
    if (numBlocks <= 0 || numBlocks > kHugeDir) {
        dierr = kDIErrBadDiskImage;
        goto bail;
    }

    fDirectory = new uint8_t[kBlkSize * numBlocks];
    if (fDirectory == NULL) {
        dierr = kDIErrMalloc;
        goto bail;
    }

    block = kVolHeaderBlock;
    dirPtr = fDirectory;
    while (numBlocks--) {
        dierr = fpImg->ReadBlock(block, dirPtr);
        if (dierr != kDIErrNone)
            goto bail;

        block++;
        dirPtr += kBlkSize;
    }

bail:
    if (dierr != kDIErrNone) {
        delete[] fDirectory;
        fDirectory = NULL;
    }
    return dierr;
}

/*
 * Write our copy of the catalog back out to disk.
 */
DIError DiskFSPascal::SaveCatalog(void)
{
    DIError dierr = kDIErrNone;
    uint8_t* dirPtr;
    int block, numBlocks;

    assert(fDirectory != NULL);

    numBlocks = fNextBlock - kVolHeaderBlock;
    block = kVolHeaderBlock;
    dirPtr = fDirectory;
    while (numBlocks--) {
        dierr = fpImg->WriteBlock(block, dirPtr);
        if (dierr != kDIErrNone)
            goto bail;

        block++;
        dirPtr += kBlkSize;
    }

bail:
    return dierr;
}

/*
 * Free the catalog storage.
 */
void DiskFSPascal::FreeCatalog(void)
{
    delete[] fDirectory;
    fDirectory = NULL;
}


/*
 * Process the catalog into A2File structures.
 */
DIError DiskFSPascal::ProcessCatalog(void)
{
    DIError dierr = kDIErrNone;
    int i, nameLen;
    A2FilePascal* pFile;
    const uint8_t* dirPtr;
    uint16_t prevNextBlock = fNextBlock;

    dierr = LoadCatalog();
    if (dierr != kDIErrNone)
        goto bail;

    dirPtr = fDirectory + kDirectoryEntryLen;       // skip vol dir entry
    for (i = 0; i < fNumFiles; i++) {
        pFile = new A2FilePascal(this);

        pFile->fStartBlock = GetShortLE(&dirPtr[0x00]);
        pFile->fNextBlock = GetShortLE(&dirPtr[0x02]);
        pFile->fFileType = (A2FilePascal::FileType) GetShortLE(&dirPtr[0x04]);
        nameLen = dirPtr[0x06] & 0x0f;
        memcpy(pFile->fFileName, &dirPtr[0x07], nameLen);
        pFile->fFileName[nameLen] = '\0';
        pFile->fBytesRemaining = GetShortLE(&dirPtr[0x16]);
        pFile->fModWhen = GetShortLE(&dirPtr[0x18]);

        /* check bytesRem before setting length field */
        if (pFile->fBytesRemaining > kBlkSize) {
            LOGI(" Pascal found strange bytesRem %u on '%s', trimming",
                pFile->fBytesRemaining, pFile->fFileName);
            pFile->fBytesRemaining = kBlkSize;
            pFile->SetQuality(A2File::kQualitySuspicious);
        }

        pFile->fLength = pFile->fBytesRemaining +
            (pFile->fNextBlock - pFile->fStartBlock -1) * kBlkSize;

        /*
         * Check values.
         */
        if (pFile->fStartBlock == pFile->fNextBlock) {
            LOGI(" Pascal found zero-block file '%s'", pFile->fFileName);
            pFile->SetQuality(A2File::kQualityDamaged);
        }
        if (pFile->fStartBlock < prevNextBlock) {
            LOGI(" Pascal start of '%s' (%d) overlaps previous end (%d)",
                pFile->fFileName, pFile->fStartBlock, prevNextBlock);
            pFile->SetQuality(A2File::kQualityDamaged);
        }

        if (pFile->fNextBlock > fpImg->GetNumBlocks()) {
            LOGI(" Pascal invalid 'next' block %d (max %ld) '%s'",
                pFile->fNextBlock, fpImg->GetNumBlocks(), pFile->fFileName);
            pFile->fStartBlock = pFile->fNextBlock = 0;
            pFile->fLength = 0;
            pFile->SetQuality(A2File::kQualityDamaged);
        } else if (pFile->fNextBlock > fTotalBlocks) {
            LOGI(" Pascal 'next' block %d exceeds max (%d) '%s'",
                pFile->fNextBlock, fTotalBlocks, pFile->fFileName);
            pFile->SetQuality(A2File::kQualitySuspicious);
        }

        //pFile->Dump();
        AddFileToList(pFile);

        dirPtr += kDirectoryEntryLen;
        prevNextBlock = pFile->fNextBlock;
    }

bail:
    FreeCatalog();
    return dierr;
}


/*
 * Create the volume usage map.  Since UCSD Pascal volumes have neither
 * in-use maps nor index blocks, this is pretty straightforward.
 */
DIError DiskFSPascal::ScanFileUsage(void)
{
    int block;

    /* start with the boot blocks */
    SetBlockUsage(0, VolumeUsage::kChunkPurposeSystem);
    SetBlockUsage(1, VolumeUsage::kChunkPurposeSystem);

    for (block = kVolHeaderBlock; block < fNextBlock; block++) {
        SetBlockUsage(block, VolumeUsage::kChunkPurposeVolumeDir);
    }

    A2FilePascal* pFile;
    pFile = (A2FilePascal*) GetNextFile(NULL);
    while (pFile != NULL) {
        for (block = pFile->fStartBlock; block < pFile->fNextBlock; block++)
            SetBlockUsage(block, VolumeUsage::kChunkPurposeUserData);

        pFile = (A2FilePascal*) GetNextFile(pFile);
    }

    return kDIErrNone;
}

/*
 * Update an entry in the volume usage map.
 */
void DiskFSPascal::SetBlockUsage(long block, VolumeUsage::ChunkPurpose purpose)
{
    VolumeUsage::ChunkState cstate;

    fVolumeUsage.GetChunkState(block, &cstate);
    if (cstate.isUsed) {
        cstate.purpose = VolumeUsage::kChunkPurposeConflict;
        LOGI(" Pascal conflicting uses for bl=%ld", block);
    } else {
        cstate.isUsed = true;
        cstate.isMarkedUsed = true;
        cstate.purpose = purpose;
    }
    fVolumeUsage.SetChunkState(block, &cstate);
}


/*
 * Test a string for validity as a Pascal volume name.
 *
 * Volume names can only be 7 characters long, but otherwise obey the same
 * rules as file names.
 */
/*static*/ bool DiskFSPascal::IsValidVolumeName(const char* name)
{
    if (name == NULL) {
        assert(false);
        return false;
    }
    if (strlen(name) > kMaxVolumeName)
        return false;
    return IsValidFileName(name);
}

/*
 * Test a string for validity as a Pascal file name.
 *
 * Filenames can be up to 15 characters long.  All characters are valid.
 * However, the system filer gets bent out of shape if you use spaces,
 * control characters, any of the wildcards ($=?), or filer meta-characters
 * (,[#:).  It also converts all alpha characters to upper case, but we can
 * take care of that later.
 */
/*static*/ bool DiskFSPascal::IsValidFileName(const char* name)
{
    assert(name != NULL);

    if (name[0] == '\0')
        return false;
    if (strlen(name) > A2FilePascal::kMaxFileName)
        return false;

    /* must be A-Z 0-9 '.' */
    while (*name != '\0') {
        if (*name <= 0x20 || *name >= 0x7f)     // no space, del, or ctrl
            return false;
        //if (*name >= 'a' && *name <= 'z')     // no lower case
        //  return false;
        if (strchr(kInvalidNameChars, *name) != NULL) // filer metacharacters
            return false;

        name++;
    }

    return true;
}

/*
 * Put a Pascal filesystem image on the specified DiskImg.
 */
DIError DiskFSPascal::Format(DiskImg* pDiskImg, const char* volName)
{
    DIError dierr = kDIErrNone;
    uint8_t blkBuf[kBlkSize];
    long formatBlocks;

    if (!IsValidVolumeName(volName))
        return kDIErrInvalidArg;

    /* set fpImg so calls that rely on it will work; we un-set it later */
    assert(fpImg == NULL);
    SetDiskImg(pDiskImg);

    LOGI(" Pascal formatting disk image");

    /* write ProDOS-style blocks */
    dierr = fpImg->OverrideFormat(fpImg->GetPhysicalFormat(),
                DiskImg::kFormatGenericProDOSOrd, fpImg->GetSectorOrder());
    if (dierr != kDIErrNone)
        goto bail;

    formatBlocks = pDiskImg->GetNumBlocks();
    if (formatBlocks != 280 && formatBlocks != 1600) {
        LOGI(" Pascal: rejecting format req blocks=%ld", formatBlocks);
        assert(false);
        return kDIErrInvalidArg;
    }

    /*
     * We should now zero out the disk blocks, but this is done automatically
     * on new disk images, so there's no need to do it here.
     */
//  dierr = fpImg->ZeroImage();
    LOGI(" Pascal  (not zeroing blocks)");

    /*
     * Start by writing blocks 0 and 1 (the boot blocks).  The file
     * APPLE3:FORMATTER.DATA holds images for 3.5" and 5.25" disks.
     */
    dierr = WriteBootBlocks();
    if (dierr != kDIErrNone)
        goto bail;

    /*
     * Write the disk volume entry.
     */
    memset(blkBuf, 0, sizeof(blkBuf));
    PutShortLE(&blkBuf[0x00], 0);       // start block
    PutShortLE(&blkBuf[0x02], 6);       // next block
    PutShortLE(&blkBuf[0x04], 0);       // "file" type
    blkBuf[0x06] = strlen(volName);
    memcpy(&blkBuf[0x07], volName, strlen(volName));
    PutShortLE(&blkBuf[0x0e], (uint16_t) pDiskImg->GetNumBlocks());
    PutShortLE(&blkBuf[0x10], 0);       // num files
    PutShortLE(&blkBuf[0x12], 0);       // last access date
    PutShortLE(&blkBuf[0x14], 0xa87b);  // last date set (Nov 7 1984)
    dierr = fpImg->WriteBlock(kVolHeaderBlock, blkBuf);
    if (dierr != kDIErrNone) {
        LOGI(" Format: block %d write failed (err=%d)",
            kVolHeaderBlock, dierr);
        goto bail;
    }

    /* check our work, and set some object fields, by reading what we wrote */
    dierr = LoadVolHeader();
    if (dierr != kDIErrNone) {
        LOGI(" GLITCH: couldn't read header we just wrote (err=%d)", dierr);
        goto bail;
    }


bail:
    SetDiskImg(NULL);        // shouldn't really be set by us
    return dierr;
}

/*
 * Blocks 0 and 1 of a 5.25" bootable Pascal disk, formatted by
 * APPLE3:FORMATTER from Pascal v1.3.
 */
const uint8_t gPascal525Block0[] = {
    0x01, 0xe0, 0x70, 0xb0, 0x04, 0xe0, 0x40, 0xb0, 0x39, 0xbd, 0x88, 0xc0,
    0x20, 0x20, 0x08, 0xa2, 0x00, 0xbd, 0x25, 0x08, 0x09, 0x80, 0x20, 0xfd,
    0xfb, 0xe8, 0xe0, 0x1d, 0xd0, 0xf3, 0xf0, 0xfe, 0xa9, 0x0a, 0x4c, 0x24,
    0xfc, 0x4d, 0x55, 0x53, 0x54, 0x20, 0x42, 0x4f, 0x4f, 0x54, 0x20, 0x46,
    0x52, 0x4f, 0x4d, 0x20, 0x53, 0x4c, 0x4f, 0x54, 0x20, 0x34, 0x2c, 0x20,
    0x35, 0x20, 0x4f, 0x52, 0x20, 0x36, 0x8a, 0x85, 0x43, 0x4a, 0x4a, 0x4a,
    0x4a, 0x09, 0xc0, 0x85, 0x0d, 0xa9, 0x5c, 0x85, 0x0c, 0xad, 0x00, 0x08,
    0xc9, 0x06, 0xb0, 0x0a, 0x69, 0x02, 0x8d, 0x00, 0x08, 0xe6, 0x3d, 0x6c,
    0x0c, 0x00, 0xa9, 0x00, 0x8d, 0x78, 0x04, 0xa9, 0x0a, 0x85, 0x0e, 0xa9,
    0x80, 0x85, 0x3f, 0x85, 0x11, 0xa9, 0x00, 0x85, 0x10, 0xa9, 0x08, 0x85,
    0x02, 0xa9, 0x02, 0x85, 0x0f, 0xa9, 0x00, 0x20, 0x4c, 0x09, 0xa2, 0x4e,
    0xa0, 0x06, 0xb1, 0x10, 0xd9, 0x39, 0x09, 0xf0, 0x2b, 0x18, 0xa5, 0x10,
    0x69, 0x1a, 0x85, 0x10, 0x90, 0x02, 0xe6, 0x11, 0xca, 0xd0, 0xe9, 0xc6,
    0x0e, 0xd0, 0xcc, 0x20, 0x20, 0x08, 0xa6, 0x43, 0xbd, 0x88, 0xc0, 0xa2,
    0x00, 0xbd, 0x2a, 0x09, 0x09, 0x80, 0x20, 0xfd, 0xfb, 0xe8, 0xe0, 0x15,
    0xd0, 0xf3, 0xf0, 0xfe, 0xc8, 0xc0, 0x13, 0xd0, 0xc9, 0xad, 0x81, 0xc0,
    0xad, 0x81, 0xc0, 0xa9, 0xd0, 0x85, 0x3f, 0xa9, 0x30, 0x85, 0x02, 0xa0,
    0x00, 0xb1, 0x10, 0x85, 0x0f, 0xc8, 0xb1, 0x10, 0x20, 0x4c, 0x09, 0xad,
    0x89, 0xc0, 0xa9, 0xd0, 0x85, 0x3f, 0xa9, 0x10, 0x85, 0x02, 0xa0, 0x00,
    0xb1, 0x10, 0x18, 0x69, 0x18, 0x85, 0x0f, 0xc8, 0xb1, 0x10, 0x69, 0x00,
    0x20, 0x4c, 0x09, 0xa5, 0x43, 0xc9, 0x50, 0xf0, 0x08, 0x90, 0x1a, 0xad,
    0x80, 0xc0, 0x6c, 0xf8, 0xff, 0xa2, 0x00, 0x8e, 0xc4, 0xfe, 0xe8, 0x8e,
    0xc6, 0xfe, 0xe8, 0x8e, 0xb6, 0xfe, 0xe8, 0x8e, 0xb8, 0xfe, 0x4c, 0xfb,
    0x08, 0xa2, 0x00, 0x8e, 0xc0, 0xfe, 0xe8, 0x8e, 0xc2, 0xfe, 0xa2, 0x04,
    0x8e, 0xb6, 0xfe, 0xe8, 0x8e, 0xb8, 0xfe, 0x4c, 0xfb, 0x08, 0x4e, 0x4f,
    0x20, 0x46, 0x49, 0x4c, 0x45, 0x20, 0x53, 0x59, 0x53, 0x54, 0x45, 0x4d,
    0x2e, 0x41, 0x50, 0x50, 0x4c, 0x45, 0x20, 0x0c, 0x53, 0x59, 0x53, 0x54,
    0x45, 0x4d, 0x2e, 0x41, 0x50, 0x50, 0x4c, 0x45, 0x4a, 0x08, 0xa5, 0x0f,
    0x29, 0x07, 0x0a, 0x85, 0x00, 0xa5, 0x0f, 0x28, 0x6a, 0x4a, 0x4a, 0x85,
    0xf0, 0xa9, 0x00, 0x85, 0x3e, 0x4c, 0x78, 0x09, 0xa6, 0x02, 0xf0, 0x22,
    0xc6, 0x02, 0xe6, 0x3f, 0xe6, 0x00, 0xa5, 0x00, 0x49, 0x10, 0xd0, 0x04,
    0x85, 0x00, 0xe6, 0xf0, 0xa4, 0x00, 0xb9, 0x8b, 0x09, 0x85, 0xf1, 0xa2,
    0x00, 0xe4, 0x02, 0xf0, 0x05, 0x20, 0x9b, 0x09, 0x90, 0xda, 0x60, 0x00,
    0x02, 0x04, 0x06, 0x08, 0x0a, 0x0c, 0x0e, 0x01, 0x03, 0x05, 0x07, 0x09,
    0x0b, 0x0d, 0x0f, 0xa6, 0x43, 0xa5, 0xf0, 0x0a, 0x0e, 0x78, 0x04, 0x20,
    0xa3, 0x0a, 0x4e, 0x78, 0x04, 0x20, 0x47, 0x0a, 0xb0, 0xfb, 0xa4, 0x2e,
    0x8c, 0x78, 0x04, 0xc4, 0xf0, 0xd0, 0xe6, 0xa5, 0x2d, 0xc5, 0xf1, 0xd0,
    0xec, 0x20, 0xdf, 0x09, 0xb0, 0xe7, 0x20, 0xc7, 0x09, 0x18, 0x60, 0xa0,
    0x00, 0xa2, 0x56, 0xca, 0x30, 0xfb, 0xb9, 0x00, 0x02, 0x5e, 0x00, 0x03,
    0x2a, 0x5e, 0x00, 0x03, 0x2a, 0x91, 0x3e, 0xc8, 0xd0, 0xed, 0x60, 0xa0,
    0x20, 0x88, 0xf0, 0x61, 0xbd, 0x8c, 0xc0, 0x10, 0xfb, 0x49, 0xd5, 0xd0,
    0xf4, 0xea, 0xbd, 0x8c, 0xc0, 0x10, 0xfb, 0xc9, 0xaa, 0xd0, 0xf2, 0xa0,
    0x56, 0xbd, 0x8c, 0xc0, 0x10, 0xfb, 0xc9, 0xad
};
const uint8_t gPascal525Block1[] = {
    0xd0, 0xe7, 0xa9, 0x00, 0x88, 0x84, 0x26, 0xbc, 0x8c, 0xc0, 0x10, 0xfb,
    0x59, 0xd6, 0x02, 0xa4, 0x26, 0x99, 0x00, 0x03, 0xd0, 0xee, 0x84, 0x26,
    0xbc, 0x8c, 0xc0, 0x10, 0xfb, 0x59, 0xd6, 0x02, 0xa4, 0x26, 0x99, 0x00,
    0x02, 0xc8, 0xd0, 0xee, 0xbc, 0x8c, 0xc0, 0x10, 0xfb, 0xd9, 0xd6, 0x02,
    0xd0, 0x13, 0xbd, 0x8c, 0xc0, 0x10, 0xfb, 0xc9, 0xde, 0xd0, 0x0a, 0xea,
    0xbd, 0x8c, 0xc0, 0x10, 0xfb, 0xc9, 0xaa, 0xf0, 0x5c, 0x38, 0x60, 0xa0,
    0xfc, 0x84, 0x26, 0xc8, 0xd0, 0x04, 0xe6, 0x26, 0xf0, 0xf3, 0xbd, 0x8c,
    0xc0, 0x10, 0xfb, 0xc9, 0xd5, 0xd0, 0xf0, 0xea, 0xbd, 0x8c, 0xc0, 0x10,
    0xfb, 0xc9, 0xaa, 0xd0, 0xf2, 0xa0, 0x03, 0xbd, 0x8c, 0xc0, 0x10, 0xfb,
    0xc9, 0x96, 0xd0, 0xe7, 0xa9, 0x00, 0x85, 0x27, 0xbd, 0x8c, 0xc0, 0x10,
    0xfb, 0x2a, 0x85, 0x26, 0xbd, 0x8c, 0xc0, 0x10, 0xfb, 0x25, 0x26, 0x99,
    0x2c, 0x00, 0x45, 0x27, 0x88, 0x10, 0xe7, 0xa8, 0xd0, 0xb7, 0xbd, 0x8c,
    0xc0, 0x10, 0xfb, 0xc9, 0xde, 0xd0, 0xae, 0xea, 0xbd, 0x8c, 0xc0, 0x10,
    0xfb, 0xc9, 0xaa, 0xd0, 0xa4, 0x18, 0x60, 0x86, 0x2b, 0x85, 0x2a, 0xcd,
    0x78, 0x04, 0xf0, 0x48, 0xa9, 0x00, 0x85, 0x26, 0xad, 0x78, 0x04, 0x85,
    0x27, 0x38, 0xe5, 0x2a, 0xf0, 0x37, 0xb0, 0x07, 0x49, 0xff, 0xee, 0x78,
    0x04, 0x90, 0x05, 0x69, 0xfe, 0xce, 0x78, 0x04, 0xc5, 0x26, 0x90, 0x02,
    0xa5, 0x26, 0xc9, 0x0c, 0xb0, 0x01, 0xa8, 0x20, 0xf4, 0x0a, 0xb9, 0x15,
    0x0b, 0x20, 0x04, 0x0b, 0xa5, 0x27, 0x29, 0x03, 0x0a, 0x05, 0x2b, 0xaa,
    0xbd, 0x80, 0xc0, 0xb9, 0x21, 0x0b, 0x20, 0x04, 0x0b, 0xe6, 0x26, 0xd0,
    0xbf, 0x20, 0x04, 0x0b, 0xad, 0x78, 0x04, 0x29, 0x03, 0x0a, 0x05, 0x2b,
    0xaa, 0xbd, 0x81, 0xc0, 0xa6, 0x2b, 0x60, 0xea, 0xa2, 0x11, 0xca, 0xd0,
    0xfd, 0xe6, 0x46, 0xd0, 0x02, 0xe6, 0x47, 0x38, 0xe9, 0x01, 0xd0, 0xf0,
    0x60, 0x01, 0x30, 0x28, 0x24, 0x20, 0x1e, 0x1d, 0x1c, 0x1c, 0x1c, 0x1c,
    0x1c, 0x70, 0x2c, 0x26, 0x22, 0x1f, 0x1e, 0x1d, 0x1c, 0x1c, 0x1c, 0x1c,
    0x1c, 0x20, 0x43, 0x4f, 0x50, 0x59, 0x52, 0x49, 0x47, 0x48, 0x54, 0x20,
    0x41, 0x50, 0x50, 0x4c, 0x45, 0x20, 0x43, 0x4f, 0x4d, 0x50, 0x55, 0x54,
    0x45, 0x52, 0x2c, 0x20, 0x49, 0x4e, 0x43, 0x2e, 0x2c, 0x20, 0x31, 0x39,
    0x38, 0x34, 0x2c, 0x20, 0x31, 0x39, 0x38, 0x35, 0x20, 0x43, 0x2e, 0x4c,
    0x45, 0x55, 0x4e, 0x47, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x68, 0x03, 0x00, 0x00, 0x02, 0x00, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xbb
};

/*
 * Block 0 of a 3.5" bootable Pascal disk, formatted by
 * APPLE3:FORMATTER from Pascal v1.3.  Block 1 is zeroed out.
 */
const uint8_t gPascal35Block0[] = {
    0x01, 0xe0, 0x70, 0xb0, 0x04, 0xe0, 0x40, 0xb0, 0x39, 0xbd, 0x88, 0xc0,
    0x20, 0x20, 0x08, 0xa2, 0x00, 0xbd, 0x25, 0x08, 0x09, 0x80, 0x20, 0xfd,
    0xfb, 0xe8, 0xe0, 0x1d, 0xd0, 0xf3, 0xf0, 0xfe, 0xa9, 0x0a, 0x4c, 0x24,
    0xfc, 0x4d, 0x55, 0x53, 0x54, 0x20, 0x42, 0x4f, 0x4f, 0x54, 0x20, 0x46,
    0x52, 0x4f, 0x4d, 0x20, 0x53, 0x4c, 0x4f, 0x54, 0x20, 0x34, 0x2c, 0x20,
    0x35, 0x20, 0x4f, 0x52, 0x20, 0x36, 0x8a, 0x85, 0x43, 0x4a, 0x4a, 0x4a,
    0x4a, 0x09, 0xc0, 0x85, 0x15, 0x8d, 0x5d, 0x09, 0xa9, 0x00, 0x8d, 0x78,
    0x04, 0x85, 0x14, 0xa9, 0x0a, 0x85, 0x0e, 0xa9, 0x80, 0x85, 0x13, 0x85,
    0x11, 0xa9, 0x00, 0x85, 0x10, 0x85, 0x0b, 0xa9, 0x02, 0x85, 0x0a, 0xa9,
    0x04, 0x85, 0x02, 0x20, 0x40, 0x09, 0xa2, 0x4e, 0xa0, 0x06, 0xb1, 0x10,
    0xd9, 0x2d, 0x09, 0xf0, 0x2b, 0x18, 0xa5, 0x10, 0x69, 0x1a, 0x85, 0x10,
    0x90, 0x02, 0xe6, 0x11, 0xca, 0xd0, 0xe9, 0xc6, 0x0e, 0xd0, 0xcc, 0x20,
    0x20, 0x08, 0xa6, 0x43, 0xbd, 0x88, 0xc0, 0xa2, 0x00, 0xbd, 0x1e, 0x09,
    0x09, 0x80, 0x20, 0xfd, 0xfb, 0xe8, 0xe0, 0x15, 0xd0, 0xf3, 0xf0, 0xfe,
    0xc8, 0xc0, 0x13, 0xd0, 0xc9, 0xad, 0x83, 0xc0, 0xad, 0x83, 0xc0, 0xa9,
    0xd0, 0x85, 0x13, 0xa0, 0x00, 0xb1, 0x10, 0x85, 0x0a, 0xc8, 0xb1, 0x10,
    0x85, 0x0b, 0xa9, 0x18, 0x85, 0x02, 0x20, 0x40, 0x09, 0xad, 0x8b, 0xc0,
    0xa9, 0xd0, 0x85, 0x13, 0xa0, 0x00, 0xb1, 0x10, 0x18, 0x69, 0x18, 0x85,
    0x0a, 0xc8, 0xb1, 0x10, 0x69, 0x00, 0x85, 0x0b, 0xa9, 0x08, 0x85, 0x02,
    0x20, 0x40, 0x09, 0xa5, 0x43, 0xc9, 0x50, 0xf0, 0x08, 0x90, 0x1a, 0xad,
    0x80, 0xc0, 0x6c, 0xf8, 0xff, 0xa2, 0x00, 0x8e, 0xc4, 0xfe, 0xe8, 0x8e,
    0xc6, 0xfe, 0xe8, 0x8e, 0xb6, 0xfe, 0xe8, 0x8e, 0xb8, 0xfe, 0x4c, 0xef,
    0x08, 0xa2, 0x00, 0x8e, 0xc0, 0xfe, 0xe8, 0x8e, 0xc2, 0xfe, 0xa2, 0x04,
    0x8e, 0xb6, 0xfe, 0xe8, 0x8e, 0xb8, 0xfe, 0x4c, 0xef, 0x08, 0x4e, 0x4f,
    0x20, 0x46, 0x49, 0x4c, 0x45, 0x20, 0x53, 0x59, 0x53, 0x54, 0x45, 0x4d,
    0x2e, 0x41, 0x50, 0x50, 0x4c, 0x45, 0x20, 0x0c, 0x53, 0x59, 0x53, 0x54,
    0x45, 0x4d, 0x2e, 0x41, 0x50, 0x50, 0x4c, 0x45, 0xa9, 0x01, 0x85, 0x42,
    0xa0, 0xff, 0xb1, 0x14, 0x8d, 0x5c, 0x09, 0xa9, 0x00, 0x85, 0x44, 0xa5,
    0x13, 0x85, 0x45, 0xa5, 0x0a, 0x85, 0x46, 0xa5, 0x0b, 0x85, 0x47, 0x20,
    0x00, 0x00, 0x90, 0x03, 0x4c, 0x5b, 0x08, 0xc6, 0x02, 0xf0, 0x0c, 0xe6,
    0x13, 0xe6, 0x13, 0xe6, 0x0a, 0xd0, 0xdc, 0xe6, 0x0b, 0xd0, 0xd8, 0x60,
    0x20, 0x43, 0x4f, 0x50, 0x59, 0x52, 0x49, 0x47, 0x48, 0x54, 0x20, 0x41,
    0x50, 0x50, 0x4c, 0x45, 0x20, 0x43, 0x4f, 0x4d, 0x50, 0x55, 0x54, 0x45,
    0x52, 0x2c, 0x20, 0x49, 0x4e, 0x43, 0x2e, 0x2c, 0x20, 0x31, 0x39, 0x38,
    0x34, 0x2c, 0x20, 0x31, 0x39, 0x38, 0x35, 0x20, 0x43, 0x2e, 0x4c, 0x45,
    0x55, 0x4e, 0x47, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0xb0, 0x01, 0x00, 0x00, 0x02, 0x00, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

/*
 * Write the Pascal boot blocks onto the disk image.
 */
DIError DiskFSPascal::WriteBootBlocks(void)
{
    DIError dierr;
    uint8_t block0[512];
    uint8_t block1[512];
    bool is525 = false;

    assert(fpImg->GetHasBlocks());
    if (fpImg->GetNumBlocks() == 280)
        is525 = true;
    else if (fpImg->GetNumBlocks() == 1600)
        is525 = false;
    else {
        LOGI(" Pascal boot blocks for blocks=%ld unknown",
            fpImg->GetNumBlocks());
        return kDIErrInternal;
    }

    if (is525) {
        memcpy(block0, gPascal525Block0, sizeof(block0));
        memcpy(block1, gPascal525Block1, sizeof(block1));
    } else {
        memcpy(block0, gPascal35Block0, sizeof(block0));
        memset(block1, 0, sizeof(block1));
    }

    dierr = fpImg->WriteBlock(0, block0);
    if (dierr != kDIErrNone) {
        LOGI(" WriteBootBlocks: block0 write failed (err=%d)", dierr);
        return dierr;
    }
    dierr = fpImg->WriteBlock(1, block1);
    if (dierr != kDIErrNone) {
        LOGI(" WriteBootBlocks: block1 write failed (err=%d)", dierr);
        return dierr;
    }

    return kDIErrNone;
}

/*
 * Scan for damaged files and conflicting file allocation entries.
 *
 * Appends some entries to the DiskImg notes, so this should only be run
 * once per DiskFS.
 *
 * Returns "true" if disk appears to be perfect, "false" otherwise.
 */
bool DiskFSPascal::CheckDiskIsGood(void)
{
    //DIError dierr;
    bool result = true;

    if (fEarlyDamage)
        result = false;

    /* (don't need to check to see if the boot blocks or disk catalog are
       marked in use -- the directory is defined by the set of blocks
       in the volume header) */

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
 * Run through the list of files and count up the free blocks.
 */
DIError DiskFSPascal::GetFreeSpaceCount(long* pTotalUnits, long* pFreeUnits,
    int* pUnitSize) const
{
    A2FilePascal* pFile;
    long freeBlocks = 0;
    uint16_t prevNextBlock = fNextBlock;

    pFile = (A2FilePascal*) GetNextFile(NULL);
    while (pFile != NULL) {
        freeBlocks += pFile->fStartBlock - prevNextBlock;
        prevNextBlock = pFile->fNextBlock;

        pFile = (A2FilePascal*) GetNextFile(pFile);
    }
    freeBlocks += fTotalBlocks - prevNextBlock;

    *pTotalUnits = fTotalBlocks;
    *pFreeUnits = freeBlocks;
    *pUnitSize = kBlockSize;

    return kDIErrNone;
}


/*
 * Normalize a Pascal path.  Used when adding files from DiskArchive.
 *
 * "*pNormalizedBufLen" is used to pass in the length of the buffer and
 * pass out the length of the string (should the buffer prove inadequate).
 */
DIError DiskFSPascal::NormalizePath(const char* path, char fssep,
    char* normalizedBuf, int* pNormalizedBufLen)
{
    DIError dierr = kDIErrNone;
    char tmpBuf[A2FilePascal::kMaxFileName+1];
    int len;

    DoNormalizePath(path, fssep, tmpBuf);
    len = strlen(tmpBuf)+1;

    if (*pNormalizedBufLen < len)
        dierr = kDIErrDataOverrun;
    else
        strcpy(normalizedBuf, tmpBuf);
    *pNormalizedBufLen = len;

    return dierr;
}

/*
 * Normalize a Pascal pathname.  Lower case becomes upper case, invalid
 * characters get stripped.
 *
 * "outBuf" must be able to hold kMaxFileName+1 characters.
 */
void DiskFSPascal::DoNormalizePath(const char* name, char fssep, char* outBuf)
{
    char* outp = outBuf;
    const char* cp;

    /* throw out leading pathname, if any */
    if (fssep != '\0') {
        cp = strrchr(name, fssep);
        if (cp != NULL)
            name = cp+1;
    }

    while (*name != '\0' && (outp - outBuf) < A2FilePascal::kMaxFileName) {
        if (*name > 0x20 && *name < 0x7f &&
            strchr(kInvalidNameChars, *name) == NULL)
        {
            *outp++ = toupper(*name);
        }

        name++;
    }

    *outp = '\0';

    if (*outBuf == '\0') {
        /* nothing left */
        strcpy(outBuf, "BLANK");
    }
}


/*
 * Create an empty file.  It doesn't look like pascal normally allows you
 * to create a zero-block file, so we create a 1-block file and set the
 * "data in last block" field to zero.
 *
 * We don't know how big the file will be, so we can't do a "best fit"
 * algorithm for placement.  Instead, we just put it in the largest
 * available free space.
 *
 * NOTE: the Pascal system will auto-delete zero-byte files.  It expects a
 * brand-new 1-block file to have a "bytes remaining" of 512.  The files
 * we create here are expected to be written to, not used as filler, so
 * this behavior is actually a *good* thing.
 */
DIError DiskFSPascal::CreateFile(const CreateParms* pParms, A2File** ppNewFile)
{
    DIError dierr = kDIErrNone;
    const bool createUnique = (GetParameter(kParm_CreateUnique) != 0);
    char normalName[A2FilePascal::kMaxFileName+1];
    A2FilePascal* pNewFile = NULL;

    if (fpImg->GetReadOnly())
        return kDIErrAccessDenied;
    if (!fDiskIsGood)
        return kDIErrBadDiskImage;

    assert(pParms != NULL);
    assert(pParms->pathName != NULL);
    assert(pParms->storageType == A2FileProDOS::kStorageSeedling);
    LOGI(" Pascal ---v--- CreateFile '%s'", pParms->pathName);

    /* compute maxFiles, which includes the vol dir header */
    int maxFiles =
        ((fNextBlock - kVolHeaderBlock) * kBlkSize) / kDirectoryEntryLen;
    if (fNumFiles >= maxFiles-1) {
        LOGI("Pascal volume directory full (%d entries)", fNumFiles);
        return kDIErrVolumeDirFull;
    }

    *ppNewFile = NULL;

    DoNormalizePath(pParms->pathName, pParms->fssep, normalName);

    /*
     * See if the file already exists.
     *
     * If "create unique" is set, we append digits until the name doesn't
     * match any others.  The name will be modified in place.
     */
    if (createUnique) {
        MakeFileNameUnique(normalName);
    } else {
        if (GetFileByName(normalName) != NULL) {
            LOGI(" Pascal create: normalized name '%s' already exists",
                normalName);
            dierr = kDIErrFileExists;
            goto bail;
        }
    }

    /*
     * Find the largest gap in the file space.
     *
     * We get an index pointer and A2File pointer to the previous entry.  If
     * the blank space is at the head of the list, prevIdx will be zero and
     * pPrevFile will be NULL.
     */
    A2FilePascal* pPrevFile;
    int prevIdx;

    dierr = FindLargestFreeArea(&prevIdx, &pPrevFile);
    if (dierr != kDIErrNone)
        goto bail;
    assert(prevIdx >= 0);

    /*
     * Make a new entry.
     */
    time_t now;
    pNewFile = new A2FilePascal(this);
    if (pNewFile == NULL) {
        dierr = kDIErrMalloc;
        goto bail;
    }
    if (pPrevFile == NULL)
        pNewFile->fStartBlock = fNextBlock;
    else
        pNewFile->fStartBlock = pPrevFile->fNextBlock;
    pNewFile->fNextBlock = pNewFile->fStartBlock +1;    // alloc 1 block
    pNewFile->fFileType = A2FilePascal::ConvertFileType(pParms->fileType);
    memset(pNewFile->fFileName, 0, A2FilePascal::kMaxFileName);
    strcpy(pNewFile->fFileName, normalName);
    pNewFile->fBytesRemaining = 0;
    now = time(NULL);
    pNewFile->fModWhen = A2FilePascal::ConvertPascalDate(now);

    pNewFile->fLength = 0;

    /*
     * Make a hole.
     */
    dierr = LoadCatalog();
    if (dierr != kDIErrNone)
        goto bail;

    if (fNumFiles > prevIdx) {
        LOGI("  Pascal sliding last %d entries down a slot",
            fNumFiles - prevIdx);
        memmove(fDirectory + (prevIdx+2) * kDirectoryEntryLen,
                fDirectory + (prevIdx+1) * kDirectoryEntryLen,
                (fNumFiles - prevIdx) * kDirectoryEntryLen);
    }

    /*
     * Fill the hole.
     */
    uint8_t* dirPtr;
    dirPtr = fDirectory + (prevIdx+1) * kDirectoryEntryLen;
    PutShortLE(&dirPtr[0x00], pNewFile->fStartBlock);
    PutShortLE(&dirPtr[0x02], pNewFile->fNextBlock);
    PutShortLE(&dirPtr[0x04], (uint16_t) pNewFile->fFileType);
    dirPtr[0x06] = (uint8_t) strlen(pNewFile->fFileName);
    memcpy(&dirPtr[0x07], pNewFile->fFileName, A2FilePascal::kMaxFileName);
    PutShortLE(&dirPtr[0x16], pNewFile->fBytesRemaining);
    PutShortLE(&dirPtr[0x18], pNewFile->fModWhen);

    /*
     * Update the #of files.
     */
    fNumFiles++;
    PutShortLE(&fDirectory[0x10], fNumFiles);

    /*
     * Flush.
     */
    dierr = SaveCatalog();
    if (dierr != kDIErrNone)
        goto bail;

    /*
     * Add to the linear file list.
     */
    InsertFileInList(pNewFile, pPrevFile);

    *ppNewFile = pNewFile;
    pNewFile = NULL;

bail:
    delete pNewFile;
    FreeCatalog();
    return dierr;
}

/*
 * Make the name pointed to by "fileName" unique.  The name should already
 * be FS-normalized, and be in a buffer that can hold at least kMaxFileName+1
 * bytes.
 *
 * (This is nearly identical to the code in the ProDOS implementation.  I'd
 * like to make it a general DiskFS function, but making the loop condition
 * work requires setting up callbacks, which isn't hard here but is a little
 * annoying in ProDOS because of the subdir buffer.  So it's cut & paste
 * for now.)
 *
 * Returns an error on failure, which should be impossible.
 */
DIError DiskFSPascal::MakeFileNameUnique(char* fileName)
{
    assert(fileName != NULL);
    assert(strlen(fileName) <= A2FilePascal::kMaxFileName);

    if (GetFileByName(fileName) == NULL)
        return kDIErrNone;

    LOGI(" Pascal   found duplicate of '%s', making unique", fileName);

    int nameLen = strlen(fileName);
    int dotOffset=0, dotLen=0;
    char dotBuf[kMaxExtensionLen+1];

    /* ensure the result will be null-terminated */
    memset(fileName + nameLen, 0, (A2FilePascal::kMaxFileName - nameLen) +1);

    /*
     * If this has what looks like a filename extension, grab it.  We want
     * to preserve ".gif", ".c", etc.
     */
    const char* cp = strrchr(fileName, '.');
    if (cp != NULL) {
        int tmpOffset = cp - fileName;
        if (tmpOffset > 0 && nameLen - tmpOffset <= kMaxExtensionLen) {
            LOGI("  Pascal   (keeping extension '%s')", cp);
            assert(strlen(cp) <= kMaxExtensionLen);
            strcpy(dotBuf, cp);
            dotOffset = tmpOffset;
            dotLen = nameLen - dotOffset;
        }
    }

    const int kMaxDigits = 999;
    int digits = 0;
    int digitLen;
    int copyOffset;
    char digitBuf[4];
    do {
        if (digits == kMaxDigits)
            return kDIErrFileExists;
        digits++;

        /* not the most efficient way to do this, but it'll do */
        sprintf(digitBuf, "%d", digits);
        digitLen = strlen(digitBuf);
        if (nameLen + digitLen > A2FilePascal::kMaxFileName)
            copyOffset = A2FilePascal::kMaxFileName - dotLen - digitLen;
        else
            copyOffset = nameLen - dotLen;
        memcpy(fileName + copyOffset, digitBuf, digitLen);
        if (dotLen != 0)
            memcpy(fileName + copyOffset + digitLen, dotBuf, dotLen);
    } while (GetFileByName(fileName) != NULL);

    LOGI(" Pascal  converted to unique name: %s", fileName);

    return kDIErrNone;
}

/*
 * Find the largest chunk of free space on the disk.
 *
 * Returns the index to the directory entry of the file immediately before
 * the chunk (where 0 is the directory header), and the corresponding
 * A2File entry.
 *
 * If there's no free space left, returns kDIErrDiskFull.
 */
DIError DiskFSPascal::FindLargestFreeArea(int *pPrevIdx, A2FilePascal** ppPrevFile)
{
    A2FilePascal* pFile;
    A2FilePascal* pPrevFile;
    uint16_t prevNextBlock = fNextBlock;
    int gapSize, maxGap, maxIndex, idx;

    maxIndex = -1;
    maxGap = 0;
    idx = 0;
    *ppPrevFile = pPrevFile = NULL;

    pFile = (A2FilePascal*) GetNextFile(NULL);
    while (pFile != NULL) {
        gapSize = pFile->fStartBlock - prevNextBlock;
        if (gapSize > maxGap) {
            maxGap = gapSize;
            maxIndex = idx;
            *ppPrevFile = pPrevFile;
        }

        idx++;
        prevNextBlock = pFile->fNextBlock;
        pPrevFile = pFile;
        pFile = (A2FilePascal*) GetNextFile(pFile);
    }

    gapSize = fTotalBlocks - prevNextBlock;
    if (gapSize > maxGap) {
        maxGap = gapSize;
        maxIndex = idx;
        *ppPrevFile = pPrevFile;
    }

    LOGI("Pascal largest gap after entry %d '%s' (size=%d)",
        maxIndex,
        *ppPrevFile != NULL ? (*ppPrevFile)->GetPathName() : "(root)",
        maxGap);
    *pPrevIdx = maxIndex;

    if (maxIndex < 0)
        return kDIErrDiskFull;
    return kDIErrNone;
}

/*
 * Delete a file.  Because Pascal doesn't have a block allocation map, this
 * is a simple matter of crunching the directory entry out.
 */
DIError DiskFSPascal::DeleteFile(A2File* pGenericFile)
{
    DIError dierr = kDIErrNone;
    A2FilePascal* pFile = (A2FilePascal*) pGenericFile;
    uint8_t* pEntry;
    int dirLen, offsetToNextEntry;

    if (pGenericFile == NULL) {
        assert(false);
        return kDIErrInvalidArg;
    }

    if (fpImg->GetReadOnly())
        return kDIErrAccessDenied;
    if (!fDiskIsGood)
        return kDIErrBadDiskImage;
    if (pGenericFile->IsFileOpen())
        return kDIErrFileOpen;

    LOGI("  Pascal deleting '%s'", pFile->GetPathName());

    dierr = LoadCatalog();
    if (dierr != kDIErrNone)
        goto bail;

    pEntry = FindDirEntry(pFile);
    if (pEntry == NULL) {
        assert(false);
        dierr = kDIErrInternal;
        goto bail;
    }
    dirLen = (fNumFiles+1) * kDirectoryEntryLen;
    offsetToNextEntry = (pEntry - fDirectory) + kDirectoryEntryLen;
    if (dirLen == offsetToNextEntry) {
        LOGI("+++ removing last entry");
    } else {
        memmove(pEntry, pEntry+kDirectoryEntryLen, dirLen - offsetToNextEntry);
    }

    assert(fNumFiles > 0);
    fNumFiles--;
    PutShortLE(&fDirectory[0x10], fNumFiles);

    dierr = SaveCatalog();
    if (dierr != kDIErrNone)
        goto bail;

    /*
     * Remove the A2File* from the list.
     */
    DeleteFileFromList(pFile);

bail:
    FreeCatalog();
    return dierr;
}

/*
 * Rename a file.
 */
DIError DiskFSPascal::RenameFile(A2File* pGenericFile, const char* newName)
{
    DIError dierr = kDIErrNone;
    A2FilePascal* pFile = (A2FilePascal*) pGenericFile;
    char normalName[A2FilePascal::kMaxFileName+1];
    uint8_t* pEntry;

    if (pFile == NULL || newName == NULL)
        return kDIErrInvalidArg;
    if (!IsValidFileName(newName))
        return kDIErrInvalidArg;
    if (fpImg->GetReadOnly())
        return kDIErrAccessDenied;
    if (!fDiskIsGood)
        return kDIErrBadDiskImage;
    /* not strictly necessary, but watch sanity check in Close/FindDirEntry */
    if (pGenericFile->IsFileOpen())
        return kDIErrFileOpen;

    DoNormalizePath(newName, '\0', normalName);

    LOGI(" Pascal renaming '%s' to '%s'", pFile->GetPathName(), normalName);

    dierr = LoadCatalog();
    if (dierr != kDIErrNone)
        goto bail;

    pEntry = FindDirEntry(pFile);
    if (pEntry == NULL) {
        assert(false);
        dierr = kDIErrInternal;
        goto bail;
    }

    pEntry[0x06] = strlen(normalName);
    memcpy(&pEntry[0x07], normalName, A2FilePascal::kMaxFileName);
    strcpy(pFile->fFileName, normalName);

    dierr = SaveCatalog();
    if (dierr != kDIErrNone)
        goto bail;

bail:
    FreeCatalog();
    return dierr;
}

/*
 * Set file info.
 *
 * Pascal does not have an aux type or access flags.  It has a file type,
 * but we don't allow the full range of ProDOS types.  Attempting to change
 * to an unsupported type results in "PDA" being used.
 */
DIError DiskFSPascal::SetFileInfo(A2File* pGenericFile, uint32_t fileType,
    uint32_t auxType, uint32_t accessFlags)
{
    DIError dierr = kDIErrNone;
    A2FilePascal* pFile = (A2FilePascal*) pGenericFile;
    uint8_t* pEntry;

    if (pFile == NULL)
        return kDIErrInvalidArg;
    if (fpImg->GetReadOnly())
        return kDIErrAccessDenied;
    if (!fDiskIsGood)
        return kDIErrBadDiskImage;

    LOGI("Pascal SetFileInfo '%s' fileType=0x%04x",
        pFile->GetPathName(), fileType);

    dierr = LoadCatalog();
    if (dierr != kDIErrNone)
        goto bail;

    pEntry = FindDirEntry(pFile);
    if (pEntry == NULL) {
        assert(false);
        dierr = kDIErrInternal;
        goto bail;
    }

    A2FilePascal::FileType newType;

    newType = A2FilePascal::ConvertFileType(fileType);
    PutShortLE(&pEntry[0x04], (uint16_t) newType);

    dierr = SaveCatalog();
    if (dierr != kDIErrNone)
        goto bail;

    /* update our local copy */
    pFile->fFileType = newType;

bail:
    FreeCatalog();
    return dierr;
}

/*
 * Change the Pascal volume name.
 */
DIError DiskFSPascal::RenameVolume(const char* newName)
{
    DIError dierr = kDIErrNone;
    char normalName[A2FilePascal::kMaxFileName+1];

    if (!IsValidVolumeName(newName))
        return kDIErrInvalidArg;
    if (fpImg->GetReadOnly())
        return kDIErrAccessDenied;

    DoNormalizePath(newName, '\0', normalName);

    dierr = LoadCatalog();
    if (dierr != kDIErrNone)
        goto bail;

    fDirectory[0x06] = strlen(normalName);
    memcpy(&fDirectory[0x07], normalName, fDirectory[0x06]);
    strcpy(fVolumeName, normalName);

    SetVolumeID();

    dierr = SaveCatalog();
    if (dierr != kDIErrNone)
        goto bail;

bail:
    FreeCatalog();
    return dierr;
}


/*
 * Find "pFile" in "fDirectory".
 */
uint8_t* DiskFSPascal::FindDirEntry(A2FilePascal* pFile)
{
    uint8_t* ptr;
    int i;

    assert(fDirectory != NULL);

    ptr = fDirectory;       // volume header; first iteration skips over it
    for (i = 0; i < fNumFiles; i++) {
        ptr += kDirectoryEntryLen;

        if (GetShortLE(&ptr[0x00]) == pFile->fStartBlock) {
            if (memcmp(&ptr[0x07], pFile->fFileName, ptr[0x06]) != 0) {
                assert(false);
                LOGI("name/block mismatch on '%s' %d",
                    pFile->GetPathName(), pFile->fStartBlock);
                return NULL;
            }
            return ptr;
        }
    }

    return NULL;
}


/*
 * ===========================================================================
 *      A2FilePascal
 * ===========================================================================
 */

/*
 * Convert Pascal file type to ProDOS file type.
 */
uint32_t A2FilePascal::GetFileType(void) const
{
    switch (fFileType) {
    case kTypeUntyped:      return 0x00;        // NON
    case kTypeXdsk:         return 0x01;        // BAD (was 0xf2 in v1.2.2)
    case kTypeCode:         return 0x02;        // PCD
    case kTypeText:         return 0x03;        // PTX
    case kTypeInfo:         return 0xf3;        // no idea
    case kTypeData:         return 0x05;        // PDA
    case kTypeGraf:         return 0xf4;        // no idea
    case kTypeFoto:         return 0x08;        // FOT
    case kTypeSecurdir:     return 0xf5;        // no idea
    default:
        LOGI("Pascal WARNING: found invalid file type %d", fFileType);
        return 0;
    }
}

/*
 * Convert a ProDOS file type to a Pascal file type.
 */
/*static*/ A2FilePascal::FileType A2FilePascal::ConvertFileType(long prodosType)
{
    FileType newType;

    switch (prodosType) {
    case 0x00:      newType = kTypeUntyped;     break;  // NON
    case 0x01:      newType = kTypeXdsk;        break;  // BAD
    case 0x02:      newType = kTypeCode;        break;  // PCD
    case 0x03:      newType = kTypeText;        break;  // PTX
    case 0xf3:      newType = kTypeInfo;        break;  // ?
    case 0x05:      newType = kTypeData;        break;  // PDA
    case 0xf4:      newType = kTypeGraf;        break;  // ?
    case 0x08:      newType = kTypeFoto;        break;  // FOT
    case 0xf5:      newType = kTypeSecurdir;    break;  // ?
    default:        newType = kTypeData;        break;  // PDA for generic
    }

    return newType;
}


/*
 * Convert from Pascal compact date format to a time_t.
 *
 *  Format yyyyyyydddddmmmm
 *  Month 0..12 (0 indicates invalid date)
 *  Day   0..31
 *  Year  0..100 (1900-1999; 100 will be rejected)
 *
 * We follow the ProDOS protocol of "year < 40 == 1900 + year".  We could
 * probably make that 1970, but the time_t epoch ends before then.
 *
 * The Pascal Filer uses a special date with the year 100 in it to indicate
 * file updates in progress.  If the system comes up and sees a file with
 * the year 100, it will assume that the file was created shortly before the
 * system crashed, and will remove the file.
 */
/*static*/ time_t A2FilePascal::ConvertPascalDate(PascalDate pascalDate)
{
    int year, month, day;

    month = pascalDate & 0x0f;
    if (!month)
        return 0;
    day = (pascalDate >> 4) & 0x1f;
    year = (pascalDate >> 9) & 0x7f;
    if (year == 100) {
        // ought to mark the file as "suspicious"?
        LOGI("Pascal WARNING: date with year=100");
    }
    if (year < 40)
        year += 100;

    struct tm tmbuf;
    time_t when;

    tmbuf.tm_sec = 0;
    tmbuf.tm_min = 0;
    tmbuf.tm_hour = 0;
    tmbuf.tm_mday = day;
    tmbuf.tm_mon = month-1;
    tmbuf.tm_year = year;
    tmbuf.tm_wday = 0;
    tmbuf.tm_yday = 0;
    tmbuf.tm_isdst = -1;        // let it figure DST and time zone
    when = mktime(&tmbuf);

    if (when == (time_t) -1)
        when = 0;
    return when;
}

/*
 * Convert a time_t to a Pascal-format date.
 *
 * CiderPress uses kDateInvalid==-1 and kDateNone==-2.
 */
/*static*/ A2FilePascal::PascalDate A2FilePascal::ConvertPascalDate(time_t unixDate)
{
    uint32_t date, year;
    struct tm* ptm;

    if (unixDate == 0 || unixDate == -1 || unixDate == -2)
        return 0;

    ptm = localtime(&unixDate);
    if (ptm == NULL)
        return 0;       // must've been invalid or unspecified

    year = ptm->tm_year;    // years since 1900
    if (year >= 100)
        year -= 100;
    if (year < 0 || year >= 100) {
        LOGW("WHOOPS: got year %u from %d", year, ptm->tm_year);
        year = 70;
    }
    date = year << 9 | (ptm->tm_mon+1) | ptm->tm_mday << 4;
    return (PascalDate) date;
}


/*
 * Return the file modification date.
 */
time_t A2FilePascal::GetModWhen(void) const
{
    return ConvertPascalDate(fModWhen);
}

/*
 * Dump the contents of the A2File structure.
 */
void A2FilePascal::Dump(void) const
{
    LOGI("A2FilePascal '%s'", fFileName);
    LOGI("  start=%d next=%d type=%d",
        fStartBlock, fNextBlock, fFileType);
    LOGI("  bytesRem=%d modWhen=0x%04x",
        fBytesRemaining, fModWhen);
}

/*
 * Not a whole lot to do, since there's no fancy index blocks.
 */
DIError A2FilePascal::Open(A2FileDescr** ppOpenFile, bool readOnly,
    bool rsrcFork /*=false*/)
{
    A2FDPascal* pOpenFile = NULL;

    if (!readOnly) {
        if (fpDiskFS->GetDiskImg()->GetReadOnly())
            return kDIErrAccessDenied;
        if (fpDiskFS->GetFSDamaged())
            return kDIErrBadDiskImage;
    }
    if (fpOpenFile != NULL)
        return kDIErrAlreadyOpen;
    if (rsrcFork)
        return kDIErrForkNotFound;

    pOpenFile = new A2FDPascal(this);

    pOpenFile->fOffset = 0;
    pOpenFile->fOpenEOF = fLength;
    pOpenFile->fOpenBlocksUsed = fNextBlock - fStartBlock;
    pOpenFile->fModified = false;

    fpOpenFile = pOpenFile;
    *ppOpenFile = pOpenFile;

    return kDIErrNone;
}


/*
 * ===========================================================================
 *      A2FDPascal
 * ===========================================================================
 */

/*
 * Read a chunk of data from the current offset.
 */
DIError A2FDPascal::Read(void* buf, size_t len, size_t* pActual)
{
    LOGD(" Pascal reading %lu bytes from '%s' (offset=%ld)",
        (unsigned long) len, fpFile->GetPathName(), (long) fOffset);

    A2FilePascal* pFile = (A2FilePascal*) fpFile;

    /* don't allow them to read past the end of the file */
    if (fOffset + (long)len > fOpenEOF) {
        if (pActual == NULL)
            return kDIErrDataUnderrun;
        len = (size_t) (fOpenEOF - fOffset);
    }
    if (pActual != NULL)
        *pActual = len;
    long incrLen = len;

    DIError dierr = kDIErrNone;
    uint8_t blkBuf[kBlkSize];
    long block = pFile->fStartBlock + (long) (fOffset / kBlkSize);
    int bufOffset = (long) (fOffset % kBlkSize);        // (& 0x01ff)
    size_t thisCount;

    if (len == 0)
        return kDIErrNone;
    assert(fOpenEOF != 0);

    while (len) {
        assert(block >= pFile->fStartBlock && block < pFile->fNextBlock);

        dierr = pFile->GetDiskFS()->GetDiskImg()->ReadBlock(block, blkBuf);
        if (dierr != kDIErrNone) {
            LOGI(" Pascal error reading file '%s'", pFile->fFileName);
            return dierr;
        }
        thisCount = kBlkSize - bufOffset;
        if (thisCount > len)
            thisCount = len;

        memcpy(buf, blkBuf + bufOffset, thisCount);
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
 *
 * We make the customary assumptions here: we're writing to a brand-new file,
 * and writing all data in one shot.  On a Pascal disk, that makes this
 * process almost embarrassingly simple.
 */
DIError A2FDPascal::Write(const void* buf, size_t len, size_t* pActual)
{
    DIError dierr = kDIErrNone;
    A2FilePascal* pFile = (A2FilePascal*) fpFile;
    DiskFSPascal* pDiskFS = (DiskFSPascal*) fpFile->GetDiskFS();
    uint8_t blkBuf[kBlkSize];
    size_t origLen = len;

    LOGD("   DOS Write len=%lu %s", (unsigned long) len, pFile->GetPathName());

    if (len >= 0x01000000) {    // 16MB
        assert(false);
        return kDIErrInvalidArg;
    }

    assert(fOffset == 0);       // big simplifying assumption
    assert(fOpenEOF == 0);      // another one
    assert(fOpenBlocksUsed == 1);
    assert(buf != NULL);

    /*
     * Verify that there's enough room between this file and the next to
     * hold the contents of the file.
     */
    long blocksNeeded, blocksAvail;
    A2FilePascal* pNextFile;
    pNextFile = (A2FilePascal*) pDiskFS->GetNextFile(pFile);
    if (pNextFile == NULL)
        blocksAvail = pDiskFS->GetTotalBlocks() - pFile->fStartBlock;
    else
        blocksAvail = pNextFile->fStartBlock - pFile->fStartBlock;

    blocksNeeded = (len + kBlkSize -1) / kBlkSize;
    LOGD("Pascal write '%s' %lu bytes: avail=%ld needed=%ld",
        pFile->GetPathName(), (unsigned long) len, blocksAvail, blocksNeeded);
    if (blocksAvail < blocksNeeded)
        return kDIErrDiskFull;

    /*
     * Write the data.
     */
    long block;
    block = pFile->fStartBlock;
    while (len != 0) {
        if (len >= (size_t) kBlkSize) {
            /* full block write */
            dierr = pDiskFS->GetDiskImg()->WriteBlock(block, buf);
            if (dierr != kDIErrNone)
                goto bail;

            len -= kBlkSize;
            buf = (uint8_t*) buf + kBlkSize;
        } else {
            /* partial block write */
            memset(blkBuf, 0, sizeof(blkBuf));
            memcpy(blkBuf, buf, len);
            dierr = pDiskFS->GetDiskImg()->WriteBlock(block, blkBuf);
            if (dierr != kDIErrNone)
                goto bail;

            len = 0;
        }

        block++;
    }

    /*
     * Update FD state.
     */
    fOpenBlocksUsed = blocksNeeded;
    fOpenEOF = origLen;
    fOffset = origLen;
    fModified = true;

bail:
    return dierr;
}

/*
 * Seek to a new offset.
 */
DIError A2FDPascal::Seek(di_off_t offset, DIWhence whence)
{
    //di_off_t fileLen = ((A2FilePascal*) fpFile)->fLength;

    switch (whence) {
    case kSeekSet:
        if (offset < 0 || offset > fOpenEOF)
            return kDIErrInvalidArg;
        fOffset = offset;
        break;
    case kSeekEnd:
        if (offset > 0 || offset < -fOpenEOF)
            return kDIErrInvalidArg;
        fOffset = fOpenEOF + offset;
        break;
    case kSeekCur:
        if (offset < -fOffset ||
            offset >= (fOpenEOF - fOffset))
        {
            return kDIErrInvalidArg;
        }
        fOffset += offset;
        break;
    default:
        assert(false);
        return kDIErrInvalidArg;
    }

    assert(fOffset >= 0 && fOffset <= fOpenEOF);
    return kDIErrNone;
}

/*
 * Return current offset.
 */
di_off_t A2FDPascal::Tell(void)
{
    return fOffset;
}

/*
 * Release file state, and tell our parent to destroy us.
 *
 * Most applications don't check the value of "Close", or call it from a
 * destructor, so we call CloseDescr whether we succeed or not.
 */
DIError A2FDPascal::Close(void)
{
    DIError dierr = kDIErrNone;
    DiskFSPascal* pDiskFS = (DiskFSPascal*) fpFile->GetDiskFS();

    if (fModified) {
        A2FilePascal* pFile = (A2FilePascal*) fpFile;
        uint8_t* pEntry;

        dierr = pDiskFS->LoadCatalog();
        if (dierr != kDIErrNone)
            goto bail;

        /*
         * Update our internal copies of stuff.
         */
        pFile->fLength = fOpenEOF;
        pFile->fNextBlock = pFile->fStartBlock + (uint16_t) fOpenBlocksUsed;
        pFile->fModWhen = A2FilePascal::ConvertPascalDate(time(NULL));

        /*
         * Update the "next block" value and the length-in-last-block.  We
         * have to scan through the directory to find our entry, rather
         * than remember an offset at "open" time, on the off chance that
         * somebody created or deleted a file after we were opened.
         */
        pEntry = pDiskFS->FindDirEntry(pFile);
        if (pEntry == NULL) {
            // we deleted an open file?
            assert(false);
            dierr = kDIErrInternal;
            goto bail;
        }
        uint16_t bytesInLastBlock;
        bytesInLastBlock = (uint16_t)pFile->fLength % kBlkSize;
        if (bytesInLastBlock == 0)
            bytesInLastBlock = 512;     // exactly filled out last block

        PutShortLE(&pEntry[0x02], pFile->fNextBlock);
        PutShortLE(&pEntry[0x16], bytesInLastBlock);
        PutShortLE(&pEntry[0x18], pFile->fModWhen);

        dierr = pDiskFS->SaveCatalog();
        if (dierr != kDIErrNone)
            goto bail;
    }

bail:
    pDiskFS->FreeCatalog();
    fpFile->CloseDescr(this);
    return dierr;
}

/*
 * Return the #of sectors/blocks in the file.
 */
long A2FDPascal::GetSectorCount(void) const
{
    A2FilePascal* pFile = (A2FilePascal*) fpFile;
    return (pFile->fNextBlock - pFile->fStartBlock) * 2;
}

long A2FDPascal::GetBlockCount(void) const
{
    A2FilePascal* pFile = (A2FilePascal*) fpFile;
    return pFile->fNextBlock - pFile->fStartBlock;
}

/*
 * Return the Nth track/sector in this file.
 */
DIError A2FDPascal::GetStorage(long sectorIdx, long* pTrack, long* pSector) const
{
    A2FilePascal* pFile = (A2FilePascal*) fpFile;
    long pascalIdx = sectorIdx / 2;
    long pascalBlock = pFile->fStartBlock + pascalIdx;
    if (pascalBlock >= pFile->fNextBlock)
        return kDIErrInvalidIndex;

    /* sparse blocks not possible on Pascal volumes */
    BlockToTrackSector(pascalBlock, (sectorIdx & 0x01) != 0, pTrack, pSector);
    return kDIErrNone;
}

/*
 * Return the Nth 512-byte block in this file.
 */
DIError A2FDPascal::GetStorage(long blockIdx, long* pBlock) const
{
    A2FilePascal* pFile = (A2FilePascal*) fpFile;
    long pascalBlock = pFile->fStartBlock + blockIdx;
    if (pascalBlock >= pFile->fNextBlock)
        return kDIErrInvalidIndex;

    *pBlock = pascalBlock;
    assert(*pBlock < pFile->GetDiskFS()->GetDiskImg()->GetNumBlocks());
    return kDIErrNone;
}
