/*
 * NuFX archive manipulation library
 * Copyright (C) 2000-2007 by Andy McFadden, All Rights Reserved.
 * This is free software; you can redistribute it and/or modify it under the
 * terms of the BSD License, see the file COPYING-LIB.
 *
 * Functions for reading from and writing to the archive.  These are
 * specialized functions that deal with byte ordering and CRC computation.
 * The functions associated with reading from an archive work equally well
 * with streaming archives.
 */
#include "NufxLibPriv.h"


/* this makes valgrind and purify happy, at some tiny cost in speed */
#define CLEAN_INIT  =0
/*#define CLEAN_INIT */


/*
 * ===========================================================================
 *      Read and write
 * ===========================================================================
 */

/*
 * Read one byte, optionally computing a CRC.
 */
uint8_t Nu_ReadOneC(NuArchive* pArchive, FILE* fp, uint16_t* pCrc)
{
    int ic;

    Assert(pArchive != NULL);
    Assert(fp != NULL);
    Assert(pCrc != NULL);

    ic = getc(fp);
    *pCrc = Nu_UpdateCRC16((uint8_t)ic, *pCrc);

    return (uint8_t) ic;
}

uint8_t Nu_ReadOne(NuArchive* pArchive, FILE* fp)
{
    uint16_t dummyCrc CLEAN_INIT;
    return Nu_ReadOneC(pArchive, fp, &dummyCrc);
}

/*
 * Write one byte, optionally computing a CRC.
 */
void Nu_WriteOneC(NuArchive* pArchive, FILE* fp, uint8_t val, uint16_t* pCrc)
{
    Assert(pArchive != NULL);
    Assert(fp != NULL);
    Assert(pCrc != NULL);

    putc(val, fp);
}

void Nu_WriteOne(NuArchive* pArchive, FILE* fp, uint8_t val)
{
    uint16_t dummyCrc CLEAN_INIT;
    Nu_WriteOneC(pArchive, fp, val, &dummyCrc);
}


/*
 * Read two little-endian bytes, optionally computing a CRC.
 */
uint16_t Nu_ReadTwoC(NuArchive* pArchive, FILE* fp, uint16_t* pCrc)
{
    int ic1, ic2;

    Assert(pArchive != NULL);
    Assert(fp != NULL);
    Assert(pCrc != NULL);

    ic1 = getc(fp);
    *pCrc = Nu_UpdateCRC16((uint8_t)ic1, *pCrc);
    ic2 = getc(fp);
    *pCrc = Nu_UpdateCRC16((uint8_t)ic2, *pCrc);

    return ic1 | ic2 << 8;
}

uint16_t Nu_ReadTwo(NuArchive* pArchive, FILE* fp)
{
    uint16_t dummyCrc CLEAN_INIT;
    return Nu_ReadTwoC(pArchive, fp, &dummyCrc);
}


/*
 * Write two little-endian bytes, optionally computing a CRC.
 */
void Nu_WriteTwoC(NuArchive* pArchive, FILE* fp, uint16_t val, uint16_t* pCrc)
{
    int ic1, ic2;

    Assert(pArchive != NULL);
    Assert(fp != NULL);
    Assert(pCrc != NULL);

    ic1 = val & 0xff;
    *pCrc = Nu_UpdateCRC16((uint8_t)ic1, *pCrc);
    ic2 = val >> 8;
    *pCrc = Nu_UpdateCRC16((uint8_t)ic2, *pCrc);

    putc(ic1, fp);
    putc(ic2, fp);
}

void Nu_WriteTwo(NuArchive* pArchive, FILE* fp, uint16_t val)
{
    uint16_t dummyCrc CLEAN_INIT;
    Nu_WriteTwoC(pArchive, fp, val, &dummyCrc);
}


/*
 * Read four little-endian bytes, optionally computing a CRC.
 */
uint32_t Nu_ReadFourC(NuArchive* pArchive, FILE* fp, uint16_t* pCrc)
{
    int ic1, ic2, ic3, ic4;

    Assert(pArchive != NULL);
    Assert(fp != NULL);
    Assert(pCrc != NULL);

    ic1 = getc(fp);
    *pCrc = Nu_UpdateCRC16((uint8_t)ic1, *pCrc);
    ic2 = getc(fp);
    *pCrc = Nu_UpdateCRC16((uint8_t)ic2, *pCrc);
    ic3 = getc(fp);
    *pCrc = Nu_UpdateCRC16((uint8_t)ic3, *pCrc);
    ic4 = getc(fp);
    *pCrc = Nu_UpdateCRC16((uint8_t)ic4, *pCrc);

    return ic1 | ic2 << 8 | (uint32_t)ic3 << 16 | (uint32_t)ic4 << 24;
}

uint32_t Nu_ReadFour(NuArchive* pArchive, FILE* fp)
{
    uint16_t dummyCrc CLEAN_INIT;
    return Nu_ReadFourC(pArchive, fp, &dummyCrc);
}


/*
 * Write four little-endian bytes, optionally computing a CRC.
 */
void Nu_WriteFourC(NuArchive* pArchive, FILE* fp, uint32_t val, uint16_t* pCrc)
{
    int ic1, ic2, ic3, ic4;

    Assert(pArchive != NULL);
    Assert(fp != NULL);
    Assert(pCrc != NULL);

    ic1 = val & 0xff;
    *pCrc = Nu_UpdateCRC16((uint8_t)ic1, *pCrc);
    ic2 = (val >> 8) & 0xff;
    *pCrc = Nu_UpdateCRC16((uint8_t)ic2, *pCrc);
    ic3 = (val >> 16) & 0xff;
    *pCrc = Nu_UpdateCRC16((uint8_t)ic3, *pCrc);
    ic4 = val >> 24;
    *pCrc = Nu_UpdateCRC16((uint8_t)ic4, *pCrc);

    putc(ic1, fp);
    putc(ic2, fp);
    putc(ic3, fp);
    putc(ic4, fp);
}

void Nu_WriteFour(NuArchive* pArchive, FILE* fp, uint32_t val)
{
    uint16_t dummyCrc CLEAN_INIT;
    Nu_WriteFourC(pArchive, fp, val, &dummyCrc);
}


/*
 * Read an 8-byte NuFX Date/Time structure.
 *
 * I've chosen *not* to filter away the Y2K differences between P8 ShrinkIt
 * and GS/ShrinkIt.  It's easy enough to deal with, and I figure the less
 * messing-with, the better.
 */
NuDateTime Nu_ReadDateTimeC(NuArchive* pArchive, FILE* fp, uint16_t* pCrc)
{
    NuDateTime temp;
    int ic;

    Assert(pArchive != NULL);
    Assert(fp != NULL);
    Assert(pCrc != NULL);

    ic = getc(fp);
    *pCrc = Nu_UpdateCRC16((uint8_t)ic, *pCrc);
    temp.second = ic;
    ic = getc(fp);
    *pCrc = Nu_UpdateCRC16((uint8_t)ic, *pCrc);
    temp.minute = ic;
    ic = getc(fp);
    *pCrc = Nu_UpdateCRC16((uint8_t)ic, *pCrc);
    temp.hour = ic;
    ic = getc(fp);
    *pCrc = Nu_UpdateCRC16((uint8_t)ic, *pCrc);
    temp.year = ic;
    ic = getc(fp);
    *pCrc = Nu_UpdateCRC16((uint8_t)ic, *pCrc);
    temp.day = ic;
    ic = getc(fp);
    *pCrc = Nu_UpdateCRC16((uint8_t)ic, *pCrc);
    temp.month = ic;
    ic = getc(fp);
    *pCrc = Nu_UpdateCRC16((uint8_t)ic, *pCrc);
    temp.extra = ic;
    ic = getc(fp);
    *pCrc = Nu_UpdateCRC16((uint8_t)ic, *pCrc);
    temp.weekDay = ic;

    return temp;
}

NuDateTime Nu_ReadDateTime(NuArchive* pArchive, FILE* fp, uint16_t* pCrc)
{
    uint16_t dummyCrc CLEAN_INIT;
    return Nu_ReadDateTimeC(pArchive, fp, &dummyCrc);
}


/*
 * Write an 8-byte NuFX Date/Time structure.
 */
void Nu_WriteDateTimeC(NuArchive* pArchive, FILE* fp, NuDateTime dateTime,
    uint16_t* pCrc)
{
    int ic;

    Assert(pArchive != NULL);
    Assert(fp != NULL);
    Assert(pCrc != NULL);

    ic = dateTime.second;
    *pCrc = Nu_UpdateCRC16((uint8_t)ic, *pCrc);
    putc(ic, fp);
    ic = dateTime.minute;
    *pCrc = Nu_UpdateCRC16((uint8_t)ic, *pCrc);
    putc(ic, fp);
    ic = dateTime.hour;
    *pCrc = Nu_UpdateCRC16((uint8_t)ic, *pCrc);
    putc(ic, fp);
    ic = dateTime.year;
    *pCrc = Nu_UpdateCRC16((uint8_t)ic, *pCrc);
    putc(ic, fp);
    ic = dateTime.day;
    *pCrc = Nu_UpdateCRC16((uint8_t)ic, *pCrc);
    putc(ic, fp);
    ic = dateTime.month;
    *pCrc = Nu_UpdateCRC16((uint8_t)ic, *pCrc);
    putc(ic, fp);
    ic = dateTime.extra;
    *pCrc = Nu_UpdateCRC16((uint8_t)ic, *pCrc);
    putc(ic, fp);
    ic = dateTime.weekDay;
    *pCrc = Nu_UpdateCRC16((uint8_t)ic, *pCrc);
    putc(ic, fp);
}

void Nu_WriteDateTime(NuArchive* pArchive, FILE* fp, NuDateTime dateTime)
{
    uint16_t dummyCrc CLEAN_INIT;
    Nu_WriteDateTimeC(pArchive, fp, dateTime, &dummyCrc);
}


/*
 * Read N bytes from the stream, optionally computing a CRC.
 */
void Nu_ReadBytesC(NuArchive* pArchive, FILE* fp, void* vbuffer, long count,
    uint16_t* pCrc)
{
    uint8_t* buffer = vbuffer;
    int ic;

    Assert(pArchive != NULL);
    Assert(fp != NULL);
    Assert(pCrc != NULL);
    Assert(buffer != NULL);
    Assert(count > 0);

    while (count--) {
        ic = getc(fp);
        *pCrc = Nu_UpdateCRC16((uint8_t)ic, *pCrc);
        *buffer++ = ic;
    }
}

void Nu_ReadBytes(NuArchive* pArchive, FILE* fp, void* vbuffer, long count)
{
    uint16_t dummyCrc CLEAN_INIT;
    Nu_ReadBytesC(pArchive, fp, vbuffer, count, &dummyCrc);
}


/*
 * Write N bytes to the stream, optionally computing a CRC.
 */
void Nu_WriteBytesC(NuArchive* pArchive, FILE* fp, const void* vbuffer,
    long count, uint16_t* pCrc)
{
    const uint8_t* buffer = vbuffer;
    int ic;

    Assert(pArchive != NULL);
    Assert(fp != NULL);
    Assert(pCrc != NULL);
    Assert(buffer != NULL);
    Assert(count > 0);

    while (count--) {
        ic = *buffer++;
        *pCrc = Nu_UpdateCRC16((uint8_t)ic, *pCrc);
        putc(ic, fp);
    }
}

void Nu_WriteBytes(NuArchive* pArchive, FILE* fp, const void* vbuffer,
    long count)
{
    uint16_t dummyCrc CLEAN_INIT;
    Nu_WriteBytesC(pArchive, fp, vbuffer, count, &dummyCrc);
}


/*
 * ===========================================================================
 *      General
 * ===========================================================================
 */

/*
 * Determine whether the stream completed the last set of operations
 * successfully.
 */
NuError Nu_HeaderIOFailed(NuArchive* pArchive, FILE* fp)
{
    if (feof(fp) || ferror(fp))
        return kNuErrFile;
    else
        return kNuErrNone;
}


/*
 * Seek around in an archive file.  If this is a streaming-mode archive,
 * we only allow forward relative seeks, which are emulated with read calls.
 *
 * The values for "ptrname" are the same as for fseek().
 */
NuError Nu_SeekArchive(NuArchive* pArchive, FILE* fp, long offset, int ptrname)
{
    if (Nu_IsStreaming(pArchive)) {
        Assert(ptrname == SEEK_CUR);
        Assert(offset >= 0);

        /* OPT: might be faster to fread a chunk at a time */
        while (offset--)
            (void) getc(fp);

        if (ferror(fp) || feof(fp))
            return kNuErrFileSeek;
    } else {
        if (fseek(fp, offset, ptrname) < 0)
            return kNuErrFileSeek;
    }

    return kNuErrNone;
}


/*
 * Rewind an archive to the start of NuFX record data.
 *
 * Note that rewind(3S) resets the error indication, but this doesn't.
 */
NuError Nu_RewindArchive(NuArchive* pArchive)
{
    Assert(pArchive != NULL);
    Assert(!Nu_IsStreaming(pArchive));

    if (Nu_SeekArchive(pArchive, pArchive->archiveFp,
                pArchive->headerOffset + kNuMasterHeaderSize, SEEK_SET) != 0)
        return kNuErrFileSeek;

    pArchive->currentOffset = pArchive->headerOffset + kNuMasterHeaderSize;
    
    return kNuErrNone;
}

