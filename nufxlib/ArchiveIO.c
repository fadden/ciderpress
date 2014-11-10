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
uchar
Nu_ReadOneC(NuArchive* pArchive, FILE* fp, ushort* pCrc)
{
    int ic;

    Assert(pArchive != nil);
    Assert(fp != nil);
    Assert(pCrc != nil);

    ic = getc(fp);
    *pCrc = Nu_UpdateCRC16((uchar)ic, *pCrc);

    return (uchar) ic;
}

uchar
Nu_ReadOne(NuArchive* pArchive, FILE* fp)
{
    ushort dummyCrc CLEAN_INIT;
    return Nu_ReadOneC(pArchive, fp, &dummyCrc);
}

/*
 * Write one byte, optionally computing a CRC.
 */
void
Nu_WriteOneC(NuArchive* pArchive, FILE* fp, uchar val, ushort* pCrc)
{
    Assert(pArchive != nil);
    Assert(fp != nil);
    Assert(pCrc != nil);

    putc(val, fp);
}

void
Nu_WriteOne(NuArchive* pArchive, FILE* fp, uchar val)
{
    ushort dummyCrc CLEAN_INIT;
    Nu_WriteOneC(pArchive, fp, val, &dummyCrc);
}


/*
 * Read two little-endian bytes, optionally computing a CRC.
 */
ushort
Nu_ReadTwoC(NuArchive* pArchive, FILE* fp, ushort* pCrc)
{
    int ic1, ic2;

    Assert(pArchive != nil);
    Assert(fp != nil);
    Assert(pCrc != nil);

    ic1 = getc(fp);
    *pCrc = Nu_UpdateCRC16((uchar)ic1, *pCrc);
    ic2 = getc(fp);
    *pCrc = Nu_UpdateCRC16((uchar)ic2, *pCrc);

    return ic1 | ic2 << 8;
}

ushort
Nu_ReadTwo(NuArchive* pArchive, FILE* fp)
{
    ushort dummyCrc CLEAN_INIT;
    return Nu_ReadTwoC(pArchive, fp, &dummyCrc);
}


/*
 * Write two little-endian bytes, optionally computing a CRC.
 */
void
Nu_WriteTwoC(NuArchive* pArchive, FILE* fp, ushort val, ushort* pCrc)
{
    int ic1, ic2;

    Assert(pArchive != nil);
    Assert(fp != nil);
    Assert(pCrc != nil);

    ic1 = val & 0xff;
    *pCrc = Nu_UpdateCRC16((uchar)ic1, *pCrc);
    ic2 = val >> 8;
    *pCrc = Nu_UpdateCRC16((uchar)ic2, *pCrc);

    putc(ic1, fp);
    putc(ic2, fp);
}

void
Nu_WriteTwo(NuArchive* pArchive, FILE* fp, ushort val)
{
    ushort dummyCrc CLEAN_INIT;
    Nu_WriteTwoC(pArchive, fp, val, &dummyCrc);
}


/*
 * Read four little-endian bytes, optionally computing a CRC.
 */
ulong
Nu_ReadFourC(NuArchive* pArchive, FILE* fp, ushort* pCrc)
{
    int ic1, ic2, ic3, ic4;

    Assert(pArchive != nil);
    Assert(fp != nil);
    Assert(pCrc != nil);

    ic1 = getc(fp);
    *pCrc = Nu_UpdateCRC16((uchar)ic1, *pCrc);
    ic2 = getc(fp);
    *pCrc = Nu_UpdateCRC16((uchar)ic2, *pCrc);
    ic3 = getc(fp);
    *pCrc = Nu_UpdateCRC16((uchar)ic3, *pCrc);
    ic4 = getc(fp);
    *pCrc = Nu_UpdateCRC16((uchar)ic4, *pCrc);

    return ic1 | ic2 << 8 | (ulong)ic3 << 16 | (ulong)ic4 << 24;
}

ulong
Nu_ReadFour(NuArchive* pArchive, FILE* fp)
{
    ushort dummyCrc CLEAN_INIT;
    return Nu_ReadFourC(pArchive, fp, &dummyCrc);
}


/*
 * Write four little-endian bytes, optionally computing a CRC.
 */
void
Nu_WriteFourC(NuArchive* pArchive, FILE* fp, ulong val, ushort* pCrc)
{
    int ic1, ic2, ic3, ic4;

    Assert(pArchive != nil);
    Assert(fp != nil);
    Assert(pCrc != nil);

    ic1 = val & 0xff;
    *pCrc = Nu_UpdateCRC16((uchar)ic1, *pCrc);
    ic2 = (val >> 8) & 0xff;
    *pCrc = Nu_UpdateCRC16((uchar)ic2, *pCrc);
    ic3 = (val >> 16) & 0xff;
    *pCrc = Nu_UpdateCRC16((uchar)ic3, *pCrc);
    ic4 = val >> 24;
    *pCrc = Nu_UpdateCRC16((uchar)ic4, *pCrc);

    putc(ic1, fp);
    putc(ic2, fp);
    putc(ic3, fp);
    putc(ic4, fp);
}

void
Nu_WriteFour(NuArchive* pArchive, FILE* fp, ulong val)
{
    ushort dummyCrc CLEAN_INIT;
    Nu_WriteFourC(pArchive, fp, val, &dummyCrc);
}


/*
 * Read an 8-byte NuFX Date/Time structure.
 *
 * I've chosen *not* to filter away the Y2K differences between P8 ShrinkIt
 * and GS/ShrinkIt.  It's easy enough to deal with, and I figure the less
 * messing-with, the better.
 */
NuDateTime
Nu_ReadDateTimeC(NuArchive* pArchive, FILE* fp, ushort* pCrc)
{
    NuDateTime temp;
    int ic;

    Assert(pArchive != nil);
    Assert(fp != nil);
    Assert(pCrc != nil);

    ic = getc(fp);
    *pCrc = Nu_UpdateCRC16((uchar)ic, *pCrc);
    temp.second = ic;
    ic = getc(fp);
    *pCrc = Nu_UpdateCRC16((uchar)ic, *pCrc);
    temp.minute = ic;
    ic = getc(fp);
    *pCrc = Nu_UpdateCRC16((uchar)ic, *pCrc);
    temp.hour = ic;
    ic = getc(fp);
    *pCrc = Nu_UpdateCRC16((uchar)ic, *pCrc);
    temp.year = ic;
    ic = getc(fp);
    *pCrc = Nu_UpdateCRC16((uchar)ic, *pCrc);
    temp.day = ic;
    ic = getc(fp);
    *pCrc = Nu_UpdateCRC16((uchar)ic, *pCrc);
    temp.month = ic;
    ic = getc(fp);
    *pCrc = Nu_UpdateCRC16((uchar)ic, *pCrc);
    temp.extra = ic;
    ic = getc(fp);
    *pCrc = Nu_UpdateCRC16((uchar)ic, *pCrc);
    temp.weekDay = ic;

    return temp;
}

NuDateTime
Nu_ReadDateTime(NuArchive* pArchive, FILE* fp, ushort* pCrc)
{
    ushort dummyCrc CLEAN_INIT;
    return Nu_ReadDateTimeC(pArchive, fp, &dummyCrc);
}


/*
 * Write an 8-byte NuFX Date/Time structure.
 */
void
Nu_WriteDateTimeC(NuArchive* pArchive, FILE* fp, NuDateTime dateTime,
    ushort* pCrc)
{
    int ic;

    Assert(pArchive != nil);
    Assert(fp != nil);
    Assert(pCrc != nil);

    ic = dateTime.second;
    *pCrc = Nu_UpdateCRC16((uchar)ic, *pCrc);
    putc(ic, fp);
    ic = dateTime.minute;
    *pCrc = Nu_UpdateCRC16((uchar)ic, *pCrc);
    putc(ic, fp);
    ic = dateTime.hour;
    *pCrc = Nu_UpdateCRC16((uchar)ic, *pCrc);
    putc(ic, fp);
    ic = dateTime.year;
    *pCrc = Nu_UpdateCRC16((uchar)ic, *pCrc);
    putc(ic, fp);
    ic = dateTime.day;
    *pCrc = Nu_UpdateCRC16((uchar)ic, *pCrc);
    putc(ic, fp);
    ic = dateTime.month;
    *pCrc = Nu_UpdateCRC16((uchar)ic, *pCrc);
    putc(ic, fp);
    ic = dateTime.extra;
    *pCrc = Nu_UpdateCRC16((uchar)ic, *pCrc);
    putc(ic, fp);
    ic = dateTime.weekDay;
    *pCrc = Nu_UpdateCRC16((uchar)ic, *pCrc);
    putc(ic, fp);
}

void
Nu_WriteDateTime(NuArchive* pArchive, FILE* fp, NuDateTime dateTime)
{
    ushort dummyCrc CLEAN_INIT;
    Nu_WriteDateTimeC(pArchive, fp, dateTime, &dummyCrc);
}


/*
 * Read N bytes from the stream, optionally computing a CRC.
 */
void
Nu_ReadBytesC(NuArchive* pArchive, FILE* fp, void* vbuffer, long count,
    ushort* pCrc)
{
    uchar* buffer = vbuffer;
    int ic;

    Assert(pArchive != nil);
    Assert(fp != nil);
    Assert(pCrc != nil);
    Assert(buffer != nil);
    Assert(count > 0);

    while (count--) {
        ic = getc(fp);
        *pCrc = Nu_UpdateCRC16((uchar)ic, *pCrc);
        *buffer++ = ic;
    }
}

void
Nu_ReadBytes(NuArchive* pArchive, FILE* fp, void* vbuffer, long count)
{
    ushort dummyCrc CLEAN_INIT;
    Nu_ReadBytesC(pArchive, fp, vbuffer, count, &dummyCrc);
}


/*
 * Write N bytes to the stream, optionally computing a CRC.
 */
void
Nu_WriteBytesC(NuArchive* pArchive, FILE* fp, const void* vbuffer, long count,
    ushort* pCrc)
{
    const uchar* buffer = vbuffer;
    int ic;

    Assert(pArchive != nil);
    Assert(fp != nil);
    Assert(pCrc != nil);
    Assert(buffer != nil);
    Assert(count > 0);

    while (count--) {
        ic = *buffer++;
        *pCrc = Nu_UpdateCRC16((uchar)ic, *pCrc);
        putc(ic, fp);
    }
}

void
Nu_WriteBytes(NuArchive* pArchive, FILE* fp, const void* vbuffer, long count)
{
    ushort dummyCrc CLEAN_INIT;
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
NuError
Nu_HeaderIOFailed(NuArchive* pArchive, FILE* fp)
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
NuError
Nu_SeekArchive(NuArchive* pArchive, FILE* fp, long offset, int ptrname)
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
NuError
Nu_RewindArchive(NuArchive* pArchive)
{
    Assert(pArchive != nil);
    Assert(!Nu_IsStreaming(pArchive));

    if (Nu_SeekArchive(pArchive, pArchive->archiveFp,
                pArchive->headerOffset + kNuMasterHeaderSize, SEEK_SET) != 0)
        return kNuErrFileSeek;

    pArchive->currentOffset = pArchive->headerOffset + kNuMasterHeaderSize;
    
    return kNuErrNone;
}

