/*
 * NuFX archive manipulation library
 * Copyright (C) 2000-2007 by Andy McFadden, All Rights Reserved.
 * This is free software; you can redistribute it and/or modify it under the
 * terms of the BSD License, see the file COPYING.LIB.
 *
 * 2IMG <-> SHK converter.  This is a practical example of using the
 * NufxLib Thread functions to add and extract data in the middle of a file.
 *
 * Conversions from 2IMG files do not work for raw nibble images.
 * Conversions from SHK archives only work if the disk image is in the
 * first record in the archive.  (This is easy to fix, but I'm trying to
 * keep it simple.)
 */
#include "NufxLib.h"
#include "Common.h"

#ifndef HAVE_STRCASECMP
static int
strcasecmp(const char *str1, const char *str2)
{
    while (*str1 && *str2 && toupper(*str1) == toupper(*str2))
        str1++, str2++;
    return (toupper(*str1) - toupper(*str2));
}
#endif


#define kTempFile       "imgconv.tmp"
#define kLocalFssep     PATH_SEP
#define false           0
#define true            (!false)


/*
 * ===========================================================================
 *      2IMG stuff
 * ===========================================================================
 */

#define kImgMagic       "2IMG"
#define kMyCreator      "NFXL"

#define kImageFormatDOS     0
#define kImageFormatProDOS  1
#define kImageFormatNibble  2

/*
 * 2IMG header definition (http://www.magnet.ch/emutech/Tech/).
 */
typedef struct ImgHeader {
    char            magic[4];
    char            creator[4];
    uint16_t        headerLen;
    uint16_t        version;
    uint32_t        imageFormat;
    uint32_t        flags;
    uint32_t        numBlocks;
    uint32_t        dataOffset;
    uint32_t        dataLen;
    uint32_t        cmntOffset;
    uint32_t        cmntLen;
    uint32_t        creatorOffset;
    uint32_t        creatorLen;
    uint32_t        spare[4];
} ImgHeader;

/*
 * Read a two-byte little-endian value.
 */
void ReadShortLE(FILE* fp, uint16_t* pBuf)
{
    *pBuf = getc(fp);
    *pBuf += (uint16_t) getc(fp) << 8;
}

/*
 * Write a two-byte little-endian value.
 */
void WriteShortLE(FILE* fp, uint16_t val)
{
    putc(val, fp);
    putc(val >> 8, fp);
}

/*
 * Read a four-byte little-endian value.
 */
void ReadLongLE(FILE* fp, uint32_t* pBuf)
{
    *pBuf = getc(fp);
    *pBuf += (uint32_t) getc(fp) << 8;
    *pBuf += (uint32_t) getc(fp) << 16;
    *pBuf += (uint32_t) getc(fp) << 24;
}

/*
 * Write a four-byte little-endian value.
 */
void WriteLongLE(FILE* fp, uint32_t val)
{
    putc(val, fp);
    putc(val >> 8, fp);
    putc(val >> 16, fp);
    putc(val >> 24, fp);
}

/*
 * Read the header from a 2IMG file.
 */
int ReadImgHeader(FILE* fp, ImgHeader* pHeader)
{
    size_t ignored;
    ignored = fread(pHeader->magic, 4, 1, fp);
    ignored = fread(pHeader->creator, 4, 1, fp);
    ReadShortLE(fp, &pHeader->headerLen);
    ReadShortLE(fp, &pHeader->version);
    ReadLongLE(fp, &pHeader->imageFormat);
    ReadLongLE(fp, &pHeader->flags);
    ReadLongLE(fp, &pHeader->numBlocks);
    ReadLongLE(fp, &pHeader->dataOffset);
    ReadLongLE(fp, &pHeader->dataLen);
    ReadLongLE(fp, &pHeader->cmntOffset);
    ReadLongLE(fp, &pHeader->cmntLen);
    ReadLongLE(fp, &pHeader->creatorOffset);
    ReadLongLE(fp, &pHeader->creatorLen);
    ReadLongLE(fp, &pHeader->spare[0]);
    ReadLongLE(fp, &pHeader->spare[1]);
    ReadLongLE(fp, &pHeader->spare[2]);
    ReadLongLE(fp, &pHeader->spare[3]);

    (void) ignored;
    if (feof(fp) || ferror(fp))
        return -1;

    if (strncmp(pHeader->magic, kImgMagic, 4) != 0) {
        fprintf(stderr, "ERROR: bad magic number on 2IMG file\n");
        return -1;
    }

    if (pHeader->version > 1) {
        fprintf(stderr, "WARNING: might not be able to handle version=%d\n",
            pHeader->version);
    }

    return 0;
}

/*
 * Write the header to a 2IMG file.
 */
int WriteImgHeader(FILE* fp, ImgHeader* pHeader)
{
    fwrite(pHeader->magic, 4, 1, fp);
    fwrite(pHeader->creator, 4, 1, fp);
    WriteShortLE(fp, pHeader->headerLen);
    WriteShortLE(fp, pHeader->version);
    WriteLongLE(fp, pHeader->imageFormat);
    WriteLongLE(fp, pHeader->flags);
    WriteLongLE(fp, pHeader->numBlocks);
    WriteLongLE(fp, pHeader->dataOffset);
    WriteLongLE(fp, pHeader->dataLen);
    WriteLongLE(fp, pHeader->cmntOffset);
    WriteLongLE(fp, pHeader->cmntLen);
    WriteLongLE(fp, pHeader->creatorOffset);
    WriteLongLE(fp, pHeader->creatorLen);
    WriteLongLE(fp, pHeader->spare[0]);
    WriteLongLE(fp, pHeader->spare[1]);
    WriteLongLE(fp, pHeader->spare[2]);
    WriteLongLE(fp, pHeader->spare[3]);

    if (ferror(fp))
        return -1;

    return 0;
}


/*
 * Dump the contents of an ImgHeader.
 */
void DumpImgHeader(ImgHeader* pHeader)
{
    printf("--- header contents:\n");
    printf("\tmagic         = '%.4s'\n", pHeader->magic);
    printf("\tcreator       = '%.4s'\n", pHeader->creator);
    printf("\theaderLen     = %d\n", pHeader->headerLen);
    printf("\tversion       = %d\n", pHeader->version);
    printf("\timageFormat   = %u\n", pHeader->imageFormat);
    printf("\tflags         = 0x%08x\n", pHeader->flags);
    printf("\tnumBlocks     = %u\n", pHeader->numBlocks);
    printf("\tdataOffset    = %u\n", pHeader->dataOffset);
    printf("\tdataLen       = %u\n", pHeader->dataLen);
    printf("\tcmntOffset    = %u\n", pHeader->cmntOffset);
    printf("\tcmntLen       = %u\n", pHeader->cmntLen);
    printf("\tcreatorOffset = %u\n", pHeader->creatorOffset);
    printf("\tcreatorLen    = %u\n", pHeader->creatorLen);
    printf("\n");
}


/*
 * ===========================================================================
 *      Main functions
 * ===========================================================================
 */

typedef enum ArchiveKind { kKindUnknown, kKindShk, kKindImg } ArchiveKind;

/*
 * This gets called when a buffer DataSource is no longer needed.
 */
NuResult FreeCallback(NuArchive* pArchive, void* args)
{
    free(args);
    return kNuOK;
}

/*
 * This gets called when an "FP" DataSource is no longer needed.
 */
NuResult FcloseCallback(NuArchive* pArchive, void* args)
{
    fclose((FILE*) args);
    return kNuOK;
}

/*
 * Create a data source for a ProDOS-ordered image.  Since this is already
 * in the correct format, we just point at the correct offset in the 2MG file.
 *
 * This supplies an FcloseCallback so that we can exercise that feature
 * of NufxLib.  We could just as easily not set it and call fclose()
 * ourselves, because the structure of this program is pretty simple.
 */
NuError CreateProdosSource(const ImgHeader* pHeader, FILE* fp,
    NuDataSource** ppDataSource)
{
    return NuCreateDataSourceForFP(kNuThreadFormatUncompressed, 0, fp,
            pHeader->dataOffset, pHeader->dataLen, FcloseCallback,ppDataSource);
}

/*
 * Create a data source for a DOS-ordered image.  This is a little harder,
 * since we have to reorder the blocks into ProDOS ordering for ShrinkIt.
 */
NuError CreateDosSource(const ImgHeader* pHeader, FILE* fp,
    NuDataSource** ppDataSource)
{
    NuError err;
    char* diskBuffer = NULL;
    long offset;

    if (pHeader->dataLen % 4096) {
        fprintf(stderr,
            "ERROR: image size must be multiple of 4096 (%u isn't)\n",
            pHeader->dataLen);
        err = kNuErrGeneric;
        goto bail;
    }

    if (fseek(fp, pHeader->dataOffset, SEEK_SET) < 0) {
        err = errno;
        perror("fseek failed");
        goto bail;
    }

    diskBuffer = malloc(pHeader->dataLen);
    if (diskBuffer == NULL) {
        fprintf(stderr, "ERROR: malloc(%u) failed\n", pHeader->dataLen);
        err = kNuErrMalloc;
        goto bail;
    }

    /*
     * Run through the image, reordering each track.  This is a
     * reversible transformation, i.e. if you do this twice you're back
     * to ProDOS ordering.
     */
    for (offset = 0; offset < pHeader->dataLen; offset += 4096) {
        size_t ignored;
        ignored = fread(diskBuffer + offset + 0x0000, 256, 1, fp);
        ignored = fread(diskBuffer + offset + 0x0e00, 256, 1, fp);
        ignored = fread(diskBuffer + offset + 0x0d00, 256, 1, fp);
        ignored = fread(diskBuffer + offset + 0x0c00, 256, 1, fp);
        ignored = fread(diskBuffer + offset + 0x0b00, 256, 1, fp);
        ignored = fread(diskBuffer + offset + 0x0a00, 256, 1, fp);
        ignored = fread(diskBuffer + offset + 0x0900, 256, 1, fp);
        ignored = fread(diskBuffer + offset + 0x0800, 256, 1, fp);
        ignored = fread(diskBuffer + offset + 0x0700, 256, 1, fp);
        ignored = fread(diskBuffer + offset + 0x0600, 256, 1, fp);
        ignored = fread(diskBuffer + offset + 0x0500, 256, 1, fp);
        ignored = fread(diskBuffer + offset + 0x0400, 256, 1, fp);
        ignored = fread(diskBuffer + offset + 0x0300, 256, 1, fp);
        ignored = fread(diskBuffer + offset + 0x0200, 256, 1, fp);
        ignored = fread(diskBuffer + offset + 0x0100, 256, 1, fp);
        ignored = fread(diskBuffer + offset + 0x0f00, 256, 1, fp);
        (void) ignored;
    }
    if (feof(fp) || ferror(fp)) {
        err = errno ? errno : -1;
        fprintf(stderr, "ERROR: failed while reading source file\n");
        goto bail;
    }

    /*
     * Create a data source for the buffer.  We set the "doClose" flag to
     * "true", so NufxLib will free the buffer for us.
     */
    err = NuCreateDataSourceForBuffer(kNuThreadFormatUncompressed, 0,
            (const uint8_t*) diskBuffer, 0, pHeader->dataLen,
            FreeCallback, ppDataSource);
    if (err == kNuErrNone)
        diskBuffer = NULL;

bail:
    if (diskBuffer != NULL)
        free(diskBuffer);
    return err;
}



/*
 * Convert a 2IMG file into a new SHK archive.
 *
 * This requires opening up the 2IMG file, verifying that it's okay, and
 * then creating a new disk image record and thread.
 */
int ConvertFromImgToShk(const char* srcName, const char* dstName)
{
    NuError err;
    NuArchive* pArchive = NULL;
    NuDataSource* pDataSource = NULL;
    NuRecordIdx recordIdx;
    NuFileDetails fileDetails;
    ImgHeader header;
    FILE* fp = NULL;
    uint32_t flushStatus;
    char* storageName = NULL;
    char* cp;

    printf("Converting 2IMG file '%s' to ShrinkIt archive '%s'\n\n",
        srcName, dstName);

    err = kNuErrGeneric;

    fp = fopen(srcName, kNuFileOpenReadOnly);
    if (fp == NULL) {
        perror("fopen failed");
        goto bail;
    }

    if (ReadImgHeader(fp, &header) < 0) {
        fprintf(stderr, "ERROR: header read failed\n");
        goto bail;
    }

    DumpImgHeader(&header);

    if (header.imageFormat != kImageFormatDOS &&
        header.imageFormat != kImageFormatProDOS)
    {
        fprintf(stderr, "ERROR: can only handle DOS and ProDOS images\n");
        goto bail;
    }

    if (header.numBlocks > 1600)
        printf("WARNING: that's a big honking image!\n");

    /*
     * Open a new archive read-write.  This refuses to overwrite an
     * existing file.
     */
    (void) unlink(kTempFile);
    err = NuOpenRW(dstName, kTempFile, kNuOpenCreat|kNuOpenExcl, &pArchive);
    if (err != kNuErrNone) {
        fprintf(stderr, "ERROR: unable to create archive (err=%d)\n", err);
        goto bail;
    }

    /* create the name that will be stored in the archive */
    storageName = strdup(dstName);
    cp = strrchr(storageName, '.');
    if (cp != NULL)
        *cp = '\0';
    cp = strrchr(storageName, kLocalFssep);
    if (cp != NULL && *(cp+1) != '\0')
        cp++;
    else
        cp = storageName;

    /*
     * We can't say "add file", because NufxLib doesn't know what a 2MG
     * archive is.  However, we can point a DataSource at the data in
     * the file, and construct the record manually.
     */

    /* set up the contents of the NuFX Record */
    memset(&fileDetails, 0, sizeof(fileDetails));
    fileDetails.storageNameMOR = cp;
    fileDetails.fileSysID = kNuFileSysUnknown;  /* DOS? ProDOS? */
    fileDetails.fileSysInfo = kLocalFssep;
    fileDetails.access = kNuAccessUnlocked;
    fileDetails.extraType = header.numBlocks;
    fileDetails.storageType = 512;
    /* FIX - ought to set the file dates */

    /* add a new record */
    err = NuAddRecord(pArchive, &fileDetails, &recordIdx);
    if (err != kNuErrNone) {
        fprintf(stderr, "ERROR: unable to create record (err=%d)\n", err);
        goto bail;
    }

    /*
     * Create a data source for the 2IMG file.  We do this differently
     * for DOS and ProDOS, because we have to rearrange the sector
     * ordering for DOS-ordered images (ShrinkIt always uses ProDOS order).
     */
    switch (header.imageFormat) {
    case kImageFormatDOS:
        err = CreateDosSource(&header, fp, &pDataSource);
        fp = NULL;
        break;
    case kImageFormatProDOS:
        err = CreateProdosSource(&header, fp, &pDataSource);
        fp = NULL;
        break;
    default:
        fprintf(stderr, "How the heck did I get here?");
        err = kNuErrInternal;
        goto bail;
    }
    if (err != kNuErrNone) {
        fprintf(stderr, "ERROR: unable to create data source (err=%d)\n", err);
        goto bail;
    }

    /* add a disk image thread */
    err = NuAddThread(pArchive, recordIdx, kNuThreadIDDiskImage, pDataSource,
            NULL);
    if (err != kNuErrNone) {
        fprintf(stderr, "ERROR: unable to create thread (err=%d)\n", err);
        goto bail;
    }
    pDataSource = NULL;  /* library owns it now */

    /* nothing happens until we Flush */
    err = NuFlush(pArchive, &flushStatus);
    if (err != kNuErrNone) {
        fprintf(stderr, "ERROR: flush failed (err=%d, status=0x%04x)\n",
            err, flushStatus);
        goto bail;
    }
    err = NuClose(pArchive);
    if (err != kNuErrNone) {
        fprintf(stderr, "ERROR: close failed (err=%d)\n", err);
        goto bail;
    }
    pArchive = NULL;

bail:
    if (pArchive != NULL) {
        (void)NuAbort(pArchive);
        (void)NuClose(pArchive);
    }
    NuFreeDataSource(pDataSource);
    if (storageName != NULL)
        free(storageName);
    if (fp != NULL)
        fclose(fp);
    return (err == kNuErrNone) ? 0 : -1;
}


/*
 * Convert an SHK archive into a 2IMG file.
 *
 * This takes a simple-minded approach and assumes that the first record
 * in the archive has the disk image in it.  If it doesn't, we give up.
 */
int ConvertFromShkToImg(const char* srcName, const char* dstName)
{
    NuError err;
    NuArchive* pArchive = NULL;
    NuDataSink* pDataSink = NULL;
    NuRecordIdx recordIdx;
    const NuRecord* pRecord;
    const NuThread* pThread = NULL;
    ImgHeader header;
    FILE* fp = NULL;
    int idx;

    printf("Converting ShrinkIt archive '%s' to 2IMG file '%s'\n\n",
        srcName, dstName);

    /*
     * Open the archive.
     */
    err = NuOpenRO(srcName, &pArchive);
    if (err != kNuErrNone) {
        fprintf(stderr, "ERROR: unable to open archive (err=%d)\n", err);
        goto bail;
    }

    /* get the first record */
    err = NuGetRecordIdxByPosition(pArchive, 0, &recordIdx);
    if (err != kNuErrNone) {
        fprintf(stderr, "ERROR: unable to get first recordIdx (err=%d)\n", err);
        goto bail;
    }
    err = NuGetRecord(pArchive, recordIdx, &pRecord);
    if (err != kNuErrNone) {
        fprintf(stderr, "ERROR: unable to get first record (err=%d)\n", err);
        goto bail;
    }

    /* find a disk image thread */
    for (idx = 0; idx < (int)NuRecordGetNumThreads(pRecord); idx++) {
        pThread = NuGetThread(pRecord, idx);

        if (NuGetThreadID(pThread) == kNuThreadIDDiskImage)
            break;
    }
    if (idx == (int)NuRecordGetNumThreads(pRecord)) {
        fprintf(stderr, "ERROR: no disk image found in first record\n");
        err = -1;
        goto bail;
    }

    /*
     * Looks good.  Open the 2IMG file, and create the header.
     */
    if (access(dstName, F_OK) == 0) {
        fprintf(stderr, "ERROR: output file already exists\n");
        err = -1;
        goto bail;
    }

    fp = fopen(dstName, kNuFileOpenWriteTrunc);
    if (fp == NULL) {
        perror("fopen failed");
        goto bail;
    }

    /* set up the 2MG header, based on the NuFX Record */
    memset(&header, 0, sizeof(header));
    memcpy(header.magic, kImgMagic, sizeof(header.magic));
    memcpy(header.creator, kMyCreator, sizeof(header.creator));
    header.headerLen = 64;
    header.version = 1;
    header.imageFormat = kImageFormatProDOS;    /* always ProDOS-order */
    header.numBlocks = pRecord->recExtraType;
    header.dataOffset = 64;
    /* old versions of ShrinkIt blew the threadEOF, so use NufxLib's "actual" */
    header.dataLen = pThread->actualThreadEOF;
    DumpImgHeader(&header);
    if (WriteImgHeader(fp, &header) < 0) {
        fprintf(stderr, "ERROR: header write failed\n");
        err = -1;
        goto bail;
    }

    /*
     * We want to expand the disk image thread into "fp" at the current
     * offset.
     */
    err = NuCreateDataSinkForFP(true, kNuConvertOff, fp, &pDataSink);
    if (err != kNuErrNone) {
        fprintf(stderr, "ERROR: unable to create data sink (err=%d)\n", err);
        goto bail;
    }

    err = NuExtractThread(pArchive, pThread->threadIdx, pDataSink);
    if (err != kNuErrNone) {
        fprintf(stderr, "ERROR: unable to extract thread (err=%d)\n", err);
        goto bail;
    }

bail:
    if (pArchive != NULL)
        NuClose(pArchive);
    NuFreeDataSink(pDataSink);
    if (fp != NULL)
        fclose(fp);
    return (err == kNuErrNone) ? 0 : -1;
}


/*
 * Figure out what kind of archive this is by looking at the filename.
 */
ArchiveKind DetermineKind(const char* filename)
{
    const char* dot;

    dot = strrchr(filename, '.');
    if (dot == NULL)
        return kKindUnknown;

    if (strcasecmp(dot, ".shk") == 0 || strcasecmp(dot, ".sdk") == 0)
        return kKindShk;
    else if (strcasecmp(dot, ".2mg") == 0)
        return kKindImg;

    return kKindUnknown;
}



/*
 * Figure out what we want to do.
 */
int main(int argc, char** argv)
{
    ArchiveKind kind;
    int cc;

    if (argc != 3) {
        fprintf(stderr, "Usage: %s (input.2mg|input.shk) output\n", argv[0]);
        exit(2);
    }

    kind = DetermineKind(argv[1]);
    if (kind == kKindUnknown) {
        fprintf(stderr, "ERROR: input name must end in '.shk' or '.2mg'\n");
        exit(2);
    }

    if (kind == kKindShk)
        cc = ConvertFromShkToImg(argv[1], argv[2]);
    else
        cc = ConvertFromImgToShk(argv[1], argv[2]);

    if (cc)
        fprintf(stderr, "Failed\n");
    else
        printf("Done!\n");

    exit(cc != 0);
}

