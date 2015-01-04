/*
 * NuFX archive manipulation library
 * Copyright (C) 2000-2007 by Andy McFadden, All Rights Reserved.
 * This is free software; you can redistribute it and/or modify it under the
 * terms of the BSD License, see the file COPYING-LIB.
 *
 * ShrinkIt LZW functions.  The original code was developed by Kent Dickey
 * and Andy Nicholas.
 *
 * Unisys holds US patent #4,558,302 (filed June 20, 1983 and issued December
 * 10, 1985).  A policy set in 1995 specifies the lifetime of a patent as
 * the longer of 20 years from the date of application or 17 years from the
 * date of grant, so the Unisys LZW patent expired on June 20, 2003 in the
 * USA.  Patents in some other countries expire after July 7, 2004.
 *
 * An older note:
 *
 * The Unisys patent is one of many that covers LZW compression, but Unisys
 * is the only company actively attacking anyone who uses it. The statement
 * Unisys made regarding LZW (and, specifically, GIF and TIFF-LZW) says:
 *
 * Q: I use LZW in my programs, but not for GIF or TIFF graphics. What should
 *    I do?
 * A: If you are not a business, and the programs are for your own personal
 *    non-commercial or not-for-profit use, Unisys does not require you to
 *    obtain a license. If they are used as part of a business and/or you sell
 *    the programs for commercial or for-profit purposes, then you must contact
 *    the Welch Patent Licensing Department at Unisys and explain your
 *    circumstances. They will have a license agreement for your application of
 *    their LZW algorithm.
 *
 * According to this, the use of LZW in NufxLib has never required a license.
 */
#include "NufxLibPriv.h"

#ifdef ENABLE_LZW

/* the LZW algorithms operate on 4K chunks */
#define kNuLZWBlockSize     4096

/* a little padding to avoid mysterious crashes on bad data */
#define kNuSafetyPadding    64

#define kNuLZWClearCode     0x0100
#define kNuLZWFirstCode     0x0101


/* sometimes we want to get *really* verbose rather late in a large archive */
#ifdef DEBUG_LZW
  static Boolean gNuDebugVerbose = true;
  #define DBUG_LZW(x)   { if (gNuDebugVerbose) { DBUG(x); } }
#else
  #define DBUG_LZW ((void)0)
#endif


/*
 * ===========================================================================
 *      Compression
 * ===========================================================================
 */

/*
 * We use a hash function borrowed from UNIX compress, which is described
 * in the v4.3 sources as:
 *
 *  Algorithm:  use open addressing double hashing (no chaining) on the
 *  prefix code / next character combination.  We do a variant of Knuth's
 *  algorithm D (vol. 3, sec. 6.4) along with G. Knott's relatively-prime
 *  secondary probe.  Here, the modular division first probe is gives way
 *  to a faster exclusive-or manipulation.
 *
 * The function used to generate it is:
 *
 *          int c, hashf[256];
 *          for (c = 256; --c >= 0; ) {
 *              hashf[c] = (((c & 0x7) << 7) ^ c) << (maxbits-10);
 *          }
 *
 * It is used with:
 *
 *          hash = prefixcode ^ hashf[c];      \* c is char from getchar() *\
 *
 * The value for kNuLZWHashSize determines the size of the hash table and
 * the % occupancy.  We want a fair number of vacancies because we probe
 * when we collide.  Using 5119 (0x13ff) with 12-bit codes yields 75%
 * occupancy.
 */

#define kNuLZWHashSize          5119    /* must be prime */
#define kNuLZWEntryUnused       0       /* indicates an unused hash entry */
#define kNuLZWHashFuncTblSize   256     /* one entry per char value */
#define kNuLZWDefaultVol        0xfe    /* use this as volume number */
#define kNuLZWHashDelta         0x120   /* used in secondary hashing */
#define kNuLZWMinCode           kNuLZWClearCode /* smallest 12-bit LZW code */
#define kNuLZWMaxCode           0x0fff  /* largest 12-bit LZW code */
#define kNuLZW2StopCode         0x0ffd  /* LZW/2 stops here */

/*
 * Mask of bits, from 0 to 8.
 */
static const int gNuBitMask[] = {
    0x00, 0x01, 0x03, 0x07, 0x0f, 0x1f, 0x3f, 0x7f, 0xff
};

#define kNuRLEDefaultEscape     0xdb    /* ShrinkIt standard */

/*
 * This holds all of the "big" dynamic state, plus a few things that I
 * don't want to pass around.  It's allocated once for each instance of
 * an open archive, and re-used.
 *
 * The hash table consists of three parts.  We have a choice for some of
 * them, "ushort" or "uint".  With "ushort" it uses less memory and is
 * more likely to fit in a CPU cache, but on some processors you have to
 * add instructions to manipulate 16-bit values in a 32-bit word.  I'm
 * guessing "ushort" is better overall.
 */
typedef struct LZWCompressState {
    NuArchive*      pArchive;

    uint16_t        entry[kNuLZWHashSize];              /* uint or ushort */
    uint16_t        prefix[kNuLZWMaxCode+1];            /* uint or ushort */
    uint8_t         suffix[kNuLZWMaxCode+1];

    uint16_t        hashFunc[kNuLZWHashFuncTblSize];    /* uint or ushort */

    uint8_t         inputBuf[kNuLZWBlockSize];      /* 4K of raw input */
    uint8_t         rleBuf[kNuLZWBlockSize*2 + kNuSafetyPadding];
    uint8_t         lzwBuf[(kNuLZWBlockSize * 3) / 2 + kNuSafetyPadding];

    uint16_t        chunkCrc;                   /* CRC for LZW/1 */

    /* LZW/2 state variables */
    int             nextFree;
    int             codeBits;
    int             highCode;
    Boolean         initialClear;
} LZWCompressState;


/*
 * Allocate some "reusable" state for LZW compression.
 *
 * The only thing that really needs to be retained across calls is
 * the hash function.  This way we don't have to re-create it for
 * every file, or store it statically in the binary.
 */
static NuError Nu_AllocLZWCompressState(NuArchive* pArchive)
{
    NuError err;
    LZWCompressState* lzwState;
    int ic;

    Assert(pArchive != NULL);
    Assert(pArchive->lzwCompressState == NULL);

    /* allocate the general-purpose compression buffer, if needed */
    err = Nu_AllocCompressionBufferIFN(pArchive);
    if (err != kNuErrNone)
        return err;

    pArchive->lzwCompressState = Nu_Malloc(pArchive, sizeof(LZWCompressState));
    if (pArchive->lzwCompressState == NULL)
        return kNuErrMalloc;

    /*
     * The "hashFunc" table only needs to be set up once.
     */
    lzwState = pArchive->lzwCompressState;
    for (ic = 256; --ic >= 0; )
        lzwState->hashFunc[ic] = (((ic & 0x7) << 7) ^ ic) << 2;

    return kNuErrNone;
}


/*
 * Compress a block of input from lzwState->inputBuf to lzwState->rleBuf.
 * The size of the output is returned in "*pRLESize" (will be zero if the
 * block expanded instead of compressing).
 *
 * The maximum possible size of the output is 2x the original, which can
 * only occur if the input is an alternating sequence of RLE delimiters
 * and non-delimiters.  It requires 3 bytes to encode a solitary 0xdb,
 * so you get (4096 / 2) non-delimiters plus (4096 / 2) * 3 RLE-encoded
 * delimiters.  We deal with this by using an 8K output buffer, so we
 * don't have to watch for overflow in the inner loop.
 *
 * The RLE format is "<delim> <char> <count>", where count is zero-based
 * (i.e. for three bytes we encode "2", allowing us to express 1-256).
 */
static NuError Nu_CompressBlockRLE(LZWCompressState* lzwState, int* pRLESize)
{
    const uint8_t* inPtr = lzwState->inputBuf;
    const uint8_t* endPtr = inPtr + kNuLZWBlockSize;
    uint8_t* outPtr = lzwState->rleBuf;
    uint8_t matchChar;
    int matchCount;

    while (inPtr < endPtr) {
        matchChar = *inPtr;
        matchCount = 1;

        /* count up the matching chars */
        while (*++inPtr == matchChar && inPtr < endPtr)
            matchCount++;

        if (matchCount > 3) {
            if (matchCount > 256) {
                /* rare case - really long match */
                while (matchCount > 256) {
                    *outPtr++ = kNuRLEDefaultEscape;
                    *outPtr++ = matchChar;
                    *outPtr++ = 255;
                    matchCount -= 256;
                }

                /* take care of the odd bits -- which might not form a run! */
                if (matchCount > 3) {
                    *outPtr++ = kNuRLEDefaultEscape;
                    *outPtr++ = matchChar;
                    *outPtr++ = matchCount -1;
                } else {
                    while (matchCount--)
                        *outPtr++ = matchChar;
                }

            } else {
                /* common case */
                *outPtr++ = kNuRLEDefaultEscape;
                *outPtr++ = matchChar;
                *outPtr++ = matchCount -1;
            }

        } else {
            if (matchChar == kNuRLEDefaultEscape) {
                /* encode 1-3 0xDBs */
                *outPtr++ = kNuRLEDefaultEscape;
                *outPtr++ = kNuRLEDefaultEscape;
                *outPtr++ = matchCount -1;
            } else {
                while (matchCount--)
                    *outPtr++ = matchChar;
            }
        }
    }

    *pRLESize = outPtr - lzwState->rleBuf;
    Assert(*pRLESize > 0 && *pRLESize < sizeof(lzwState->rleBuf));

    return kNuErrNone;
}


/*
 * Clear the LZW table.  Also resets the LZW/2 state.
 */
static void Nu_ClearLZWTable(LZWCompressState* lzwState)
{
    Assert(lzwState != NULL);

    /*DBUG_LZW(("### clear table\n"));*/

    /* reset table entries */
    Assert(kNuLZWEntryUnused == 0);     /* make sure this is okay */
    memset(lzwState->entry, 0, sizeof(lzwState->entry));

    /* reset state variables */
    lzwState->nextFree = kNuLZWFirstCode;
    lzwState->codeBits = 9;
    lzwState->highCode = ~(~0 << lzwState->codeBits);   /* a/k/a 0x01ff */
    lzwState->initialClear = false;
}


/*
 * Write a variable-width LZW code to the output.  "prefixCode" has the
 * value to write, and "codeBits" is the width.
 *
 * Data is written in little-endian order (lowest byte first).  The
 * putcode function in LZC is probably faster, but the format isn't
 * compatible with SHK.
 *
 * The worst conceivable expansion for LZW is 12 bits of output for every
 * byte of input.  Because we're using variable-width codes and LZW is
 * reasonably effective at finding matches, the actual expansion will
 * certainly be less.  Throwing the extra 2K onto the end of the buffer
 * saves us from having to check for a buffer overflow here.
 *
 * On exit, "*pOutBuf" will point PAST the last byte we wrote (even if
 * it's a partial byte), and "*pAtBit" will contain the bit offset.
 *
 * (Turning this into a macro might speed things up.)
 */
static inline void Nu_LZWPutCode(uint8_t** pOutBuf, uint32_t prefixCode,
    int codeBits, int* pAtBit)
{
    int atBit = *pAtBit;
    uint8_t* outBuf = *pOutBuf;

    /*DBUG_LZW(("### PUT: prefixCode=0x%04lx, codeBits=%d, atBit=%d\n",
        prefixCode, codeBits, atBit));*/

    Assert(atBit >= 0 && atBit < sizeof(gNuBitMask));

    if (atBit) {
        /* align the prefix code with the existing byte */
        prefixCode <<= atBit;

        /* merge it with the buffer contents (if necessary) and write lo bits */
        outBuf--;
        *outBuf = (uint8_t)((*outBuf & gNuBitMask[atBit]) | prefixCode);
        outBuf++;
    } else {
        /* nothing to merge with; write lo byte at next posn and advance */
        *outBuf++ = (uint8_t)prefixCode;
    }

    /* codes are at least 9 bits, so we know we have to write one more */
    *outBuf++ = (uint8_t)(prefixCode >> 8);

    /* in some cases, we may have to write yet another */
    atBit += codeBits;
    if (atBit > 16)
        *outBuf++ = (uint8_t)(prefixCode >> 16);
    
    *pAtBit = atBit & 0x07;
    *pOutBuf = outBuf;
}


/*
 * Compress a block of data with LZW, from "inputBuf" to lzwState->lzwBuf.
 *
 * LZW/1 is just like LZW/2, except that for the former the table is
 * always cleared before this function is called.  Because of this, the
 * table never fills completely, so none of the table-overflow code
 * ever happens.
 *
 * This function is patterned after the LZC compress function, rather
 * than the NuLib LZW code, because the NuLib code was abysmal (a rather
 * straight translation from 6502 assembly).  This function differs from LZC
 * in a few areas in order to make the output match GS/ShrinkIt.
 *
 * There is a (deliberate) minor bug here: if a table clear is emitted
 * when there is only one character left in the input, nothing will be
 * added to the hash table (as there is nothing to add) but "nextFree"
 * will be advanced.  This mimics GSHK's behavior, and accounts for the
 * "resetFix" logic in the expansion functions.  Code 0x0101 is essentially
 * lost in this situation.
 */
static NuError Nu_CompressLZWBlock(LZWCompressState* lzwState,
    const uint8_t* inputBuf, int inputCount, int* pOutputCount)
{
    int nextFree, ic, atBit, codeBits;
    int hash, hashDelta;
    int prefixCode, code, highCode;
    const uint8_t* inputEnd = inputBuf + inputCount;
    /* local copies of lzwState members, for speed */
    const uint16_t* pHashFunc = lzwState->hashFunc;
    uint16_t* pEntry = lzwState->entry;
    uint16_t* pPrefix = lzwState->prefix;
    uint8_t* pSuffix = lzwState->suffix;
    uint8_t* outBuf = lzwState->lzwBuf;

    Assert(lzwState != NULL);
    Assert(inputBuf != NULL);
    Assert(inputCount > 0 && inputCount <= kNuLZWBlockSize);
    /* make sure nobody has been messing with the types */
    Assert(sizeof(pHashFunc[0]) == sizeof(lzwState->hashFunc[0]));
    Assert(sizeof(pEntry[0]) == sizeof(lzwState->entry[0]));
    Assert(sizeof(pPrefix[0]) == sizeof(lzwState->prefix[0]));
    Assert(sizeof(pSuffix[0]) == sizeof(lzwState->suffix[0]));

    /*DBUG_LZW(("### START LZW (nextFree=0x%04x)\n", lzwState->nextFree));*/

    atBit = 0;

    if (lzwState->initialClear) {
        /*DBUG_LZW(("### initialClear set\n"));*/
        codeBits = lzwState->codeBits;
        Nu_LZWPutCode(&outBuf, kNuLZWClearCode, codeBits, &atBit);
        Nu_ClearLZWTable(lzwState);
    }

  table_cleared:
    /* recover our state (or get newly-cleared state) */
    nextFree = lzwState->nextFree;
    codeBits = lzwState->codeBits;
    highCode = lzwState->highCode;

    prefixCode = *inputBuf++;

    /*DBUG_LZW(("### fchar=0x%02x\n", prefixCode));*/

    while (inputBuf < inputEnd) {
        ic = *inputBuf++;
        /*DBUG_LZW(("### char=0x%02x\n", ic));*/

        hash = prefixCode ^ pHashFunc[ic];
        code = pEntry[hash];

        if (code != kNuLZWEntryUnused) {
            /* something is here, either our prefix or a hash collision */
            if (pSuffix[code] != ic || pPrefix[code] != prefixCode) {
                /* we've collided; do the secondary probe */
                hashDelta = (kNuLZWHashDelta - ic) << 2;
                do {
                    /* rehash and keep looking */
                    Assert(code >= kNuLZWMinCode && code <= kNuLZWMaxCode);
                    if (hash >= hashDelta)
                        hash -= hashDelta;
                    else
                        hash += kNuLZWHashSize - hashDelta;
                    Assert(hash >= 0 && hash < kNuLZWHashSize);

                    if ((code = pEntry[hash]) == kNuLZWEntryUnused)
                        goto new_code;
                } while (pSuffix[code] != ic || pPrefix[code] != prefixCode);
            }

            /* else we found a matching string, and can keep searching */
            prefixCode = code;

        } else {
            /* found an empty entry, add the prefix+suffix to the table */
  new_code:
            Nu_LZWPutCode(&outBuf, prefixCode, codeBits, &atBit);
            Assert(outBuf < lzwState->lzwBuf + sizeof(lzwState->lzwBuf));
            /*DBUG_LZW(("### outBuf now at +%d\n",outBuf - lzwState->lzwBuf));*/

            code = nextFree;
            Assert(hash < kNuLZWHashSize);
            Assert(code >= kNuLZWMinCode);
            Assert(code <= kNuLZWMaxCode);

            /*
             * GSHK accepts 0x0ffd, and then sends the table clear
             * immediately.  We could improve on GSHK's compression slightly
             * by using the entire table, but I want to generate the exact
             * same output as GSHK.  (The decoder believes the table clear
             * is entry 0xffe, so we've got one more coming, and possibly
             * two if we tweak getcode slightly.)
             *
             * Experiments show that switching to 0xffe increases the size
             * of files that don't compress well, and decreases the size
             * of files that do.  In both cases, the difference in size
             * is very small.
             */
            Assert(code <= kNuLZW2StopCode);
            /*if (code <= kNuLZW2StopCode) {*/
            /*DBUG_LZW(("###  added new code 0x%04x prefix=0x%04x ch=0x%02x\n",
                code, prefixCode, ic));*/

            pEntry[hash] = code;
            pPrefix[code] = prefixCode;
            pSuffix[code] = ic;

            /*
             * Check and see if it's time to increase the code size (note
             * we flip earlier than LZC by one here).
             */
            if (code >= highCode) {
                highCode += code +1;
                codeBits++;
            }

            nextFree++;

            /*}*/

            prefixCode = ic;

            /* if the table is full, clear it (only for LZW/2) */
            if (code == kNuLZW2StopCode) {
                /* output last code */
                Nu_LZWPutCode(&outBuf, prefixCode, codeBits, &atBit);

                if (inputBuf < inputEnd) {
                    /* still have data, keep going */
                    Nu_LZWPutCode(&outBuf, kNuLZWClearCode, codeBits, &atBit);
                    Nu_ClearLZWTable(lzwState);
                    goto table_cleared;
                } else {
                    /* no more input, hold table clear for next block */
                    DBUG(("--- RARE: block-end clear\n"));
                    lzwState->initialClear = true;
                    goto table_clear_finish;
                }
            }

            Assert(nextFree <= kNuLZW2StopCode);
        }
    }

    /*
     * Output the last code.  Since there's no following character, we don't
     * need to add an entry to the table... whatever we've found is already
     * in there.
     */
    Nu_LZWPutCode(&outBuf, prefixCode, codeBits, &atBit);

    /*
     * Update the counters so LZW/2 has continuity.
     */
    Assert(nextFree <= kNuLZW2StopCode);
    if (nextFree >= highCode) {
        highCode += nextFree +1;
        codeBits++;
    }
    nextFree++;     /* make room for the code we just wrote */

    if (nextFree > kNuLZW2StopCode) {
        /*
         * The code we just wrote, which was part of a longer string already
         * in the tree, took the last entry in the table.  We need to clear
         * the table, but we can't do it in this block.  We will have to
         * emit a table clear as the very first thing in the next block.
         */
        DBUG(("--- RARE: block-end inter clear\n"));
        lzwState->initialClear = true;
    }
  table_clear_finish:

    /* save state for next pass through */
    lzwState->nextFree = nextFree;
    lzwState->codeBits = codeBits;
    lzwState->highCode = highCode;

    Assert(inputBuf == inputEnd);

    *pOutputCount = outBuf - lzwState->lzwBuf;

    /*
    if (*pOutputCount < inputCount) {
        DBUG_LZW(("### compressed from %d to %d\n", inputCount, *pOutputCount));
    } else {
        DBUG_LZW(("### NO compression (%d to %d)\n", inputCount,*pOutputCount));
    }
    */

    return kNuErrNone;
}

/*
 * Compress ShrinkIt-style "LZW/1" and "LZW/2".
 *
 * "*pThreadCrc" should already be set to its initial value.  On exit it
 * will contain the CRC of the uncompressed data.
 *
 * On exit, the output file will be positioned past the last byte written.
 */
static NuError Nu_CompressLZW(NuArchive* pArchive, NuStraw* pStraw, FILE* fp,
    uint32_t srcLen, uint32_t* pDstLen, uint16_t* pThreadCrc, Boolean isType2)
{
    NuError err = kNuErrNone;
    LZWCompressState* lzwState;
    long initialOffset;
    const uint8_t* lzwInputBuf;
    uint32_t blockSize, rleSize, lzwSize;
    long compressedLen;
    Boolean keepLzw;

    Assert(pArchive != NULL);
    Assert(pStraw != NULL);
    Assert(fp != NULL);
    Assert(srcLen > 0);
    Assert(pDstLen != NULL);
    Assert(pThreadCrc != NULL);
    Assert(isType2 == true || isType2 == false);

    /*
     * Do some initialization and set-up.
     */
    if (pArchive->lzwCompressState == NULL) {
        err = Nu_AllocLZWCompressState(pArchive);
        BailError(err);
    }
    Assert(pArchive->lzwCompressState != NULL);
    Assert(pArchive->compBuf != NULL);

    lzwState = pArchive->lzwCompressState;
    lzwState->pArchive = pArchive;
    compressedLen = 0;

    /*
     * And now for something ugly: for LZW/1 we have to compute the CRC
     * twice.  Old versions of ShrinkIt used LZW/1 and put the CRC in
     * the compressed block while newer versions used LZW/2 and put the
     * CRC in the thread header.  We're using LZW/1 with the newer record
     * format, so we need two CRCs.  For some odd reason Andy N. decided
     * to use 0xffff as the initial value for the thread one, so we can't
     * just store the same thing in two places.
     *
     * Of course, this also means that an LZW/2 chunk stored in an old
     * pre-v3 record wouldn't have a CRC at all...
     *
     * LZW/1 is included here for completeness.  I can't think of a reason
     * why you'd want to use it, really.
     */
    lzwState->chunkCrc = kNuInitialChunkCRC;        /* 0x0000 */

    /*
     * An LZW/1 file starts off with a CRC of the data, which means we
     * have to compress the whole thing, then seek back afterward and
     * write the value.  This annoyance went away in LZW/2.
     */
    err = Nu_FTell(fp, &initialOffset);
    BailError(err);

    if (!isType2) {
        putc(0, fp);        /* leave space for CRC */
        putc(0, fp);
        compressedLen += 2;
    }
    putc(kNuLZWDefaultVol, fp);
    putc(kNuRLEDefaultEscape, fp);
    compressedLen += 2;

    if (isType2)
        Nu_ClearLZWTable(lzwState);

    while (srcLen) {
        /*
         * Fill up the input buffer.
         */
        blockSize = (srcLen > kNuLZWBlockSize) ? kNuLZWBlockSize : srcLen;

        err = Nu_StrawRead(pArchive, pStraw, lzwState->inputBuf, blockSize);
        if (err != kNuErrNone) {
            Nu_ReportError(NU_BLOB, err, "compression read failed");
            goto bail;
        }

        /*
         * ShrinkIt was originally just going to be a 5.25" disk compressor,
         * so the compression functions were organized around 4K blocks (the
         * size of one track on a 5.25" disk).  The block passed into the
         * RLE function is always 4K, so we zero out any extra space.
         */
        if (blockSize < kNuLZWBlockSize) {
            memset(lzwState->inputBuf + blockSize, 0,
                kNuLZWBlockSize - blockSize);
        }

        /*
         * Compute the CRC.  For LZW/1 this is on the entire 4K block, for
         * the "version 3" thread header CRC this is on just the "real" data.
         */
        *pThreadCrc = Nu_CalcCRC16(*pThreadCrc, lzwState->inputBuf, blockSize);
        if (!isType2) {
            lzwState->chunkCrc = Nu_CalcCRC16(lzwState->chunkCrc,
                lzwState->inputBuf, kNuLZWBlockSize);
        }

        /*
         * Try to compress with RLE, from inputBuf to rleBuf.
         */
        err = Nu_CompressBlockRLE(lzwState, (int*) &rleSize);
        BailError(err);

        if (rleSize < kNuLZWBlockSize) {
            lzwInputBuf = lzwState->rleBuf;
        } else {
            lzwInputBuf = lzwState->inputBuf;
            rleSize = kNuLZWBlockSize;
        }

        /*
         * Compress with LZW, into lzwBuf.
         */
        if (!isType2)
            Nu_ClearLZWTable(lzwState);
        err = Nu_CompressLZWBlock(lzwState, lzwInputBuf, rleSize,
                (int*) &lzwSize);
        BailError(err);

        /* decide if we want to keep it, bearing in mind the LZW/2 header */
        if (pArchive->valMimicSHK) {
            /* GSHK doesn't factor in header -- and *sometimes* uses "<=" !! */
            keepLzw = (lzwSize < rleSize);
        } else {
            if (isType2)
                keepLzw = (lzwSize +2 < rleSize);
            else
                keepLzw = (lzwSize < rleSize);
        }

        /*
         * Write the compressed (or not) chunk.
         */
        if (keepLzw) {
            /*
             * LZW succeeded.
             */
            if (isType2)
                rleSize |= 0x8000;      /* for LZW/2, set "LZW used" flag */

            putc(rleSize & 0xff, fp);   /* size after RLE */
            putc(rleSize >> 8, fp);
            compressedLen += 2;

            if (isType2) {
                /* write compressed LZW len (+4 for header bytes) */
                putc((lzwSize+4) & 0xff, fp);
                putc((lzwSize+4) >> 8, fp);
                compressedLen += 2;
            } else {
                /* set LZW/1 "LZW used" flag */
                putc(1, fp);
                compressedLen++;
            }
        
            /* write data from LZW buffer */
            err = Nu_FWrite(fp, lzwState->lzwBuf, lzwSize);
            BailError(err);
            compressedLen += lzwSize;
        } else {
            /*
             * LZW failed.
             */
            putc(rleSize & 0xff, fp);   /* size after RLE */
            putc(rleSize >> 8, fp);
            compressedLen += 2;

            if (isType2) {
                /* clear LZW/2 table; we can't use it next time */
                Nu_ClearLZWTable(lzwState);
            } else {
                /* set LZW/1 "LZW not used" flag */
                putc(0, fp);
                compressedLen++;
            }

            /* write data from RLE or plain-input buffer */
            err = Nu_FWrite(fp, lzwInputBuf, rleSize);
            BailError(err);
            compressedLen += rleSize;
        }


        /*
         * Update the counter and continue.
         */
        srcLen -= blockSize;
    }

    /*
     * For LZW/1, go back and write the CRC.
     */
    if (!isType2) {
        long curOffset;

        err = Nu_FTell(fp, &curOffset);
        BailError(err);
        err = Nu_FSeek(fp, initialOffset, SEEK_SET);
        BailError(err);
        putc(lzwState->chunkCrc & 0xff, fp);
        putc(lzwState->chunkCrc >> 8, fp);
        err = Nu_FSeek(fp, curOffset, SEEK_SET);
        BailError(err);
    }

    /* P8SHK and GSHK add an extra byte to LZW-compressed threads */
    if (pArchive->valMimicSHK) {
        putc(0, fp);
        compressedLen++;
    }

    *pDstLen = compressedLen;

bail:
    return err;
}

/*
 * Compress ShrinkIt-style "LZW/1".
 */
NuError Nu_CompressLZW1(NuArchive* pArchive, NuStraw* pStraw, FILE* fp,
    uint32_t srcLen, uint32_t* pDstLen, uint16_t* pCrc)
{
    return Nu_CompressLZW(pArchive, pStraw, fp, srcLen, pDstLen, pCrc, false);
}

/*
 * Compress ShrinkIt-style "LZW/2".
 */
NuError Nu_CompressLZW2(NuArchive* pArchive, NuStraw* pStraw, FILE* fp,
    uint32_t srcLen, uint32_t* pDstLen, uint16_t* pCrc)
{
    return Nu_CompressLZW(pArchive, pStraw, fp, srcLen, pDstLen, pCrc, true);
}


/*
 * ===========================================================================
 *      Expansion
 * ===========================================================================
 */

/* if we don't have at least this much data, we try to read more */
/* (the "+3" is for the chunk header bytes) */
#define kNuLZWDesiredChunk  (kNuLZWBlockSize + 3)

/*
 * Static tables useful for bit manipulation.
 */
static const uint32_t gNuMaskTable[17] = {
    0x0000, 0x01ff, 0x03ff, 0x03ff,  0x07ff, 0x07ff, 0x07ff, 0x07ff,
    0x0fff, 0x0fff, 0x0fff, 0x0fff,  0x0fff, 0x0fff, 0x0fff, 0x0fff,
    0x0fff
};
/* convert high byte of "entry" into a bit width */
static const uint32_t gNuBitWidth[17] = {
    8,9,10,10,11,11,11,11,12,12,12,12,12,12,12,12,12
};


/* entry in the trie */
typedef struct TableEntry {
    uint8_t         ch;
    uint32_t        prefix;
} TableEntry;

/*
 * This holds all of the "big" dynamic state, plus a few things that I
 * don't want to pass around.  It's allocated once for each instance of
 * an open archive, and re-used.
 */
typedef struct LZWExpandState {
    NuArchive*      pArchive;

    TableEntry      trie[4096-256];     /* holds from 9 bits to 12 bits */
    uint8_t         stack[kNuLZWBlockSize];

    // some of these don't need to be 32 bits; they were "uint" before
    uint32_t        entry;              /* 16-bit index into table */
    uint32_t        oldcode;            /* carryover state for LZW/2 */
    uint32_t        incode;             /* carryover state for LZW/2 */
    uint32_t        finalc;             /* carryover state for LZW/2 */
    Boolean         resetFix;           /* work around an LZW/2 bug */

    uint16_t        chunkCrc;           /* CRC we calculate for LZW/1 */
    uint16_t        fileCrc;            /* CRC stored with file */

    uint8_t         diskVol;            /* disk volume # */
    uint8_t         rleEscape;          /* RLE escape char, usually 0xdb */

    uint32_t        dataInBuffer;       /* #of bytes in compBuf */
    uint8_t*        dataPtr;            /* current data offset */

    uint8_t         lzwOutBuf[kNuLZWBlockSize + kNuSafetyPadding];
    uint8_t         rleOutBuf[kNuLZWBlockSize + kNuSafetyPadding];
} LZWExpandState;


/*
 * Allocate some "reusable" state for LZW expansion.
 */
static NuError Nu_AllocLZWExpandState(NuArchive* pArchive)
{
    NuError err;

    Assert(pArchive != NULL);
    Assert(pArchive->lzwExpandState == NULL);

    /* allocate the general-purpose compression buffer, if needed */
    err = Nu_AllocCompressionBufferIFN(pArchive);
    if (err != kNuErrNone)
        return err;

    pArchive->lzwExpandState = Nu_Malloc(pArchive, sizeof(LZWExpandState));
    if (pArchive->lzwExpandState == NULL)
        return kNuErrMalloc;
    return kNuErrNone;
}


#ifdef NDEBUG
# define Nu_LZWPush(uch)    ( *stackPtr++ = (uch) )
# define Nu_LZWPop()        ( *(--stackPtr) )
# define Nu_LZWStackEmpty() ( stackPtr == lzwState->stack )

#else
# define Nu_LZWPush(uch)    \
            ( Nu_LZWPushCheck(uch, lzwState, stackPtr), *stackPtr++ = (uch) )
# define Nu_LZWPop()        \
            ( Nu_LZWPopCheck(lzwState, stackPtr), *(--stackPtr) )
# define Nu_LZWStackEmpty() ( stackPtr == lzwState->stack )

static inline void Nu_LZWPushCheck(uint8_t uch, const LZWExpandState* lzwState,
    const uint8_t* stackPtr)
{
    if (stackPtr >= lzwState->stack + sizeof(lzwState->stack)) {
        Nu_ReportError(lzwState->NU_BLOB, kNuErrBadData, "stack overflow");
        abort();
    }
}

static inline void Nu_LZWPopCheck(const LZWExpandState* lzwState,
    const uint8_t* stackPtr)
{
    if (stackPtr == lzwState->stack) {
        Nu_ReportError(lzwState->NU_BLOB, kNuErrBadData, "stack underflow");
        abort();
    }
}

#endif

/*
 * Get the next LZW code from the input, advancing pointers as needed.
 *
 * This would be faster as a macro and less ugly with pass-by-reference.
 * Resorting to globals is unacceptable.  Might be less ugly if we clumped
 * some stuff into a struct.  Should be good enough as-is.
 *
 * Returns an integer up to 12 bits long.
 *
 * (Turning this into a macro might speed things up.)
 */
static inline uint32_t Nu_LZWGetCode(const uint8_t** pInBuf, uint32_t entry,
    int* pAtBit, uint32_t* pLastByte)
{
    uint32_t numBits, startBit, lastBit;
    uint32_t value;

    numBits = (entry +1) >> 8;      /* bit-width of next code */
    startBit = *pAtBit;
    lastBit = startBit + gNuBitWidth[numBits];

    /*
     * We need one or two bytes from the input.  These have to be shifted
     * around and merged with the bits we already have (if any).
     */
    if (!startBit)
        value = *(*pInBuf)++;
    else
        value = *pLastByte;

    if (lastBit > 16) {
        /* need two more bytes */
        value |= *(*pInBuf)++ << 8;
        *pLastByte = *(*pInBuf)++;
        value |= (uint32_t) *pLastByte << 16;
    } else {
        /* only need one more byte */
        *pLastByte = *(*pInBuf)++;
        value |= *pLastByte << 8;
    }

    *pAtBit = lastBit & 0x07;

    /*printf("| EX: value=$%06lx mask=$%04x return=$%03lx\n",
        value,gNuMaskTable[numBits], (value >> startBit) & gNuMaskTable[numBits]);*/

    /*DBUG_LZW(("### getcode 0x%04lx\n",
        (value >> startBit) & gNuMaskTable[numBits]));*/

    /* I believe ANSI allows shifting by zero bits, so don't test "!startBit" */
    return (value >> startBit) & gNuMaskTable[numBits];
}


/*
 * Expand an LZW/1 chunk.
 *
 * Reads from lzwState->dataPtr, writes to lzwState->lzwOutBuf.
 */
static NuError Nu_ExpandLZW1(LZWExpandState* lzwState, uint32_t expectedLen)
{
    NuError err = kNuErrNone;
    TableEntry* tablePtr;
    int atBit;
    uint32_t entry, oldcode, incode, ptr;
    uint32_t lastByte, finalc;
    const uint8_t* inbuf;
    uint8_t* outbuf;
    uint8_t* outbufend;
    uint8_t* stackPtr;

    Assert(lzwState != NULL);
    Assert(expectedLen > 0 && expectedLen <= kNuLZWBlockSize);

    inbuf = lzwState->dataPtr;
    outbuf = lzwState->lzwOutBuf;
    outbufend = outbuf + expectedLen;
    tablePtr = lzwState->trie - 256;    /* don't store 256 empties */
    stackPtr = lzwState->stack;

    atBit = 0;
    lastByte = 0;

    entry = kNuLZWFirstCode;    /* 0x101 */
    finalc = oldcode = incode = Nu_LZWGetCode(&inbuf, entry, &atBit, &lastByte);
    *outbuf++ = incode;
    Assert(incode <= 0xff);
    if (incode > 0xff) {
        err = kNuErrBadData;
        Nu_ReportError(lzwState->NU_BLOB, err, "invalid initial LZW symbol");
        goto bail;
    }

    while (outbuf < outbufend) {
        incode = ptr = Nu_LZWGetCode(&inbuf, entry, &atBit, &lastByte);

        /* handle KwKwK case */
        if (ptr >= entry) {
            //DBUG_LZW(("### KwKwK (ptr=%d entry=%d)\n", ptr, entry));
            if (ptr != entry) {
                /* bad code -- this would make us read uninitialized data */
                DBUG(("--- bad code (ptr=%d entry=%d)\n", ptr, entry));
                err = kNuErrBadData;
                return err;
            }
            Nu_LZWPush((uint8_t)finalc);
            ptr = oldcode;
        }

        /* fill the stack by chasing up the trie */
        while (ptr > 0xff) {
            Nu_LZWPush(tablePtr[ptr].ch);
            ptr = tablePtr[ptr].prefix;
            Assert(ptr < 4096);
        }

        /* done chasing up, now dump the stack, starting with ptr */
        finalc = ptr;
        *outbuf++ = ptr;
        /*printf("PUT 0x%02x\n", *(outbuf-1));*/
        while (!Nu_LZWStackEmpty()) {
            *outbuf++ = Nu_LZWPop();
            /*printf("POP/PUT 0x%02x\n", *(outbuf-1));*/
        }

        /* add the new prefix to the trie -- last string plus new char */
        Assert(finalc <= 0xff);
        tablePtr[entry].ch = finalc;
        tablePtr[entry].prefix = oldcode;
        entry++;
        oldcode = incode;
    }

bail:
    if (outbuf != outbufend) {
        err = kNuErrBadData;
        Nu_ReportError(lzwState->NU_BLOB, err, "LZW expansion failed");
        return err;
    }

    /* adjust input buffer */
    lzwState->dataInBuffer -= (inbuf - lzwState->dataPtr);
    Assert(lzwState->dataInBuffer < 32767*65536);
    lzwState->dataPtr = (uint8_t*)inbuf;

    return err;
}

/*
 * Expand an LZW/2 chunk.  Main difference from LZW/1 is that the state
 * is carried over from the previous block in most cases, and the table
 * is cleared explicitly.
 *
 * Reads from lzwState->dataPtr, writes to lzwState->lzwOutBuf.
 *
 * In some cases, "expectedInputUsed" will be -1 to indicate that the
 * value is not known.
 */
static NuError Nu_ExpandLZW2(LZWExpandState* lzwState, uint32_t expectedLen,
    uint32_t expectedInputUsed)
{
    NuError err = kNuErrNone;
    TableEntry* tablePtr;
    int atBit;
    uint32_t entry, oldcode, incode, ptr;
    uint32_t lastByte, finalc;
    const uint8_t* inbuf;
    const uint8_t* inbufend;
    uint8_t* outbuf;
    uint8_t* outbufend;
    uint8_t* stackPtr;

    /*DBUG_LZW(("### LZW/2 block start (compIn=%d, rleOut=%d, entry=0x%04x)\n",
        expectedInputUsed, expectedLen, lzwState->entry));*/
    Assert(lzwState != NULL);
    Assert(expectedLen > 0 && expectedLen <= kNuLZWBlockSize);

    inbuf = lzwState->dataPtr;
    inbufend = lzwState->dataPtr + expectedInputUsed;
    outbuf = lzwState->lzwOutBuf;
    outbufend = outbuf + expectedLen;
    entry = lzwState->entry;
    tablePtr = lzwState->trie - 256;    /* don't store 256 empties */
    stackPtr = lzwState->stack;

    atBit = 0;
    lastByte = 0;

    /*
     * If the table isn't empty, initialize from the saved state and
     * jump straight into the main loop.
     *
     * There's a funny situation that arises when a table clear is the
     * second-to-last code in the previous chunk.  After we see the
     * table clear, we get the next code and use it to initialize "oldcode"
     * and "incode" -- but we don't advance "entry" yet.  The way that
     * ShrinkIt originally worked, the next time we came through we'd
     * see what we thought was an empty table and we'd reinitialize.  So
     * we use "resetFix" to keep track of this situation.
     */
    if (entry != kNuLZWFirstCode || lzwState->resetFix) {
        /* table not empty */
        oldcode = lzwState->oldcode;
        incode = lzwState->incode;
        finalc = lzwState->finalc;
        lzwState->resetFix = false;
        goto main_loop;
    }

clear_table:
    /* table is either empty or was just explicitly cleared; reset */
    entry = kNuLZWFirstCode;    /* 0x0101 */
    if (outbuf == outbufend) {
        /* block must've ended on a table clear */
        DBUG(("--- RARE: ending clear\n"));
        /* reset values, mostly to quiet gcc's "used before init" warnings */
        oldcode = incode = finalc = 0;
        goto main_loop; /* the while condition will fall through */
    }
    finalc = oldcode = incode = Nu_LZWGetCode(&inbuf, entry, &atBit, &lastByte);
    *outbuf++ = incode;
    /*printf("PUT 0x%02x\n", *(outbuf-1));*/
    if (incode > 0xff) {
        err = kNuErrBadData;
        Nu_ReportError(lzwState->NU_BLOB, err, "invalid initial LZW symbol");
        goto bail;
    }

    if (outbuf == outbufend) {
        /* if we're out of data, raise the "reset fix" flag */
        DBUG(("--- RARE: resetFix!\n"));
        lzwState->resetFix = true;
        /* fall through; the while condition will let us slip past */
    }

main_loop:
    while (outbuf < outbufend) {
        incode = ptr = Nu_LZWGetCode(&inbuf, entry, &atBit, &lastByte);
        //DBUG_LZW(("### read incode=0x%04x\n", incode));
        if (incode == kNuLZWClearCode)      /* table clear - 0x0100 */
            goto clear_table;

        /* handle KwKwK case */
        if (ptr >= entry) {
            //DBUG_LZW(("### KwKwK (ptr=%d entry=%d)\n", ptr, entry));
            if (ptr != entry) {
                /* bad code -- this would make us read uninitialized data */
                DBUG(("--- bad code (ptr=%d entry=%d)\n", ptr, entry));
                err = kNuErrBadData;
                return err;
            }
            Nu_LZWPush((uint8_t)finalc);
            ptr = oldcode;
        }

        /* fill the stack by chasing up the trie */
        while (ptr > 0xff) {
            Nu_LZWPush(tablePtr[ptr].ch);
            ptr = tablePtr[ptr].prefix;
            Assert(ptr < 4096);
        }

        /* done chasing up, now dump the stack, starting with ptr */
        finalc = ptr;
        *outbuf++ = ptr;
        /*printf("PUT 0x%02x\n", *(outbuf-1));*/
        while (!Nu_LZWStackEmpty()) {
            *outbuf++ = Nu_LZWPop();
            /*printf("POP/PUT 0x%02x\n", *(outbuf-1));*/
        }

        /* add the new prefix to the trie -- last string plus new char */
        /*DBUG_LZW(("###  entry 0x%04x gets prefix=0x%04x and ch=0x%02x\n",
            entry, oldcode, finalc));*/
        Assert(finalc <= 0xff);
        tablePtr[entry].ch = finalc;
        tablePtr[entry].prefix = oldcode;
        entry++;
        oldcode = incode;
    }

bail:
    /*DBUG_LZW(("### end of block\n"));*/
    if (expectedInputUsed != (uint32_t) -1 && inbuf != inbufend) {
        /* data was corrupted; if we keep going this will get worse */
        DBUG(("--- inbuf != inbufend in ExpandLZW2 (diff=%d)\n",
            inbufend - inbuf));
        err = kNuErrBadData;
        return err;
    }
    Assert(outbuf == outbufend);

    /* adjust input buffer */
    lzwState->dataInBuffer -= (inbuf - lzwState->dataPtr);
    Assert(lzwState->dataInBuffer < 32767*65536);
    lzwState->dataPtr = (uint8_t*)inbuf;

    /* save off local copies of stuff */
    lzwState->entry = entry;
    lzwState->oldcode = oldcode;
    lzwState->incode = incode;
    lzwState->finalc = finalc;

    return err;
}


/*
 * Expands a chunk of RLEd data into 4K of output.
 */
static NuError Nu_ExpandRLE(LZWExpandState* lzwState, const uint8_t* inbuf,
    uint32_t expectedInputUsed)
{
    NuError err = kNuErrNone;
    uint8_t *outbuf;
    uint8_t *outbufend;
    const uint8_t *inbufend;
    uint8_t uch, rleEscape;
    int count;

    outbuf = lzwState->rleOutBuf;
    outbufend = outbuf + kNuLZWBlockSize;
    inbufend = inbuf + expectedInputUsed;
    rleEscape = lzwState->rleEscape;

    while (outbuf < outbufend) {
        uch = *inbuf++;
        if (uch == rleEscape) {
            uch = *inbuf++;
            count = *inbuf++;
            if (outbuf + count >= outbufend) {
                /* don't overrun buffer */
                Assert(outbuf != outbufend);
                break;
            }
            while (count-- >= 0)
                *outbuf++ = uch;
        } else {
            *outbuf++ = uch;
        }
    }

    if (outbuf != outbufend) {
        err = kNuErrBadData;
        Nu_ReportError(lzwState->NU_BLOB, err,
            "RLE output glitch (off by %d)", (int)(outbufend-outbuf));
        goto bail;
    }
    if (inbuf != inbufend) {
        err = kNuErrBadData;
        Nu_ReportError(lzwState->NU_BLOB, err,
            "RLE input glitch (off by %d)", (int)(inbufend-inbuf));
        goto bail;
    }

bail:
    return err;
}


/*
 * Utility function to get a byte from the input buffer.
 */
static inline uint8_t Nu_GetHeaderByte(LZWExpandState* lzwState)
{
    lzwState->dataInBuffer--;
    Assert(lzwState->dataInBuffer > 0);
    return *lzwState->dataPtr++;
}

/*
 * Expand ShrinkIt-style "LZW/1" and "LZW/2".
 *
 * This manages the input data buffer, passing chunks of compressed data
 * into the appropriate expansion function.
 *
 * Pass in NULL for "pThreadCrc" if no thread CRC is desired.  Otherwise,
 * "*pThreadCrc" should already be set to its initial value.  On exit it
 * will contain the CRC of the uncompressed data.
 */
NuError Nu_ExpandLZW(NuArchive* pArchive, const NuRecord* pRecord,
    const NuThread* pThread, FILE* infp, NuFunnel* pFunnel,
    uint16_t* pThreadCrc)
{
    NuError err = kNuErrNone;
    Boolean isType2;
    LZWExpandState* lzwState;
    uint32_t compRemaining, uncompRemaining, minSize;

    Assert(pArchive != NULL);
    Assert(pThread != NULL);
    Assert(infp != NULL);
    Assert(pFunnel != NULL);

    /*
     * Do some initialization and set-up.
     */
    if (pArchive->lzwExpandState == NULL) {
        err = Nu_AllocLZWExpandState(pArchive);
        BailError(err);
    }
    Assert(pArchive->lzwExpandState != NULL);
    Assert(pArchive->compBuf != NULL);

    lzwState = pArchive->lzwExpandState;
    lzwState->pArchive = pArchive;

    if (pThread->thThreadFormat == kNuThreadFormatLZW1) {
        isType2 = false;
        minSize = 7;    /* crc-lo,crc-hi,vol,rle-delim,len-lo,len-hi,lzw-used */
        lzwState->chunkCrc = kNuInitialChunkCRC;        /* 0x0000 */
    } else if (pThread->thThreadFormat == kNuThreadFormatLZW2) {
        isType2 = true;
        minSize = 4;    /* vol,rle-delim,len-lo,len-hi */
    } else {
        err = kNuErrBadFormat;
        goto bail;
    }

    uncompRemaining = pThread->actualThreadEOF;
    compRemaining = pThread->thCompThreadEOF;
    if (compRemaining < minSize) {
        err = kNuErrBadData;
        Nu_ReportError(NU_BLOB, err, "thread too short to be valid LZW");
        goto bail;
    }
    if (compRemaining && !uncompRemaining) {
        err = kNuErrBadData;
        Nu_ReportError(NU_BLOB, err,
            "compressed data but no uncompressed data??");
        goto bail;
    }

    /*
     * Read the LZW header out of the data stream.
     */
    if (!isType2) {
        lzwState->fileCrc = getc(infp);
        lzwState->fileCrc |= getc(infp) << 8;
        compRemaining -= 2;
    }
    lzwState->diskVol = getc(infp);     /* disk volume #; not really used */
    lzwState->rleEscape = getc(infp);   /* RLE escape char for this thread */
    compRemaining -= 2;

    lzwState->dataInBuffer = 0;
    lzwState->dataPtr = NULL;

    /* reset pointers */
    lzwState->entry = kNuLZWFirstCode;  /* 0x0101 */
    lzwState->resetFix = false;

    /*DBUG_LZW(("### LZW%d block, vol=0x%02x, rleEsc=0x%02x\n",
        isType2 +1, lzwState->diskVol, lzwState->rleEscape));*/

    /*
     * Read large blocks of the source file into compBuf, taking care not
     * to read past the end of the thread data.
     *
     * The motivation for doing it this way rather than just reading the
     * next compressed chunk are (1) compBuf is considerably larger than
     * stdio BUFSIZ on most systems, and (2) for LZW/1 we don't know the
     * size of the compressed data anyway.
     *
     * We need to ensure that we have at least one full compressed chunk
     * in the buffer.  Since the compressor will refuse to store the
     * compressed data if it grows, we know that we need 4K plus the
     * chunk header.
     *
     * Once we have what looks like a full chunk, invoke the LZW decoder.
     */
    while (uncompRemaining) {
        Boolean rleUsed;
        Boolean lzwUsed;
        uint32_t getSize;
        uint32_t rleLen;        /* length after RLE; 4096 if no RLE */
        uint32_t lzwLen = 0;    /* type 2 only */
        uint32_t writeLen, inCount;
        const uint8_t* writeBuf;

        /* if we're low, and there's more data available, read more */
        if (lzwState->dataInBuffer < kNuLZWDesiredChunk && compRemaining) {
            /*
             * First thing we do is slide the old data to the start of
             * the buffer.
             */
            if (lzwState->dataInBuffer) {
                Assert(lzwState->dataPtr != NULL);
                Assert(pArchive->compBuf != lzwState->dataPtr);
                memmove(pArchive->compBuf, lzwState->dataPtr,
                    lzwState->dataInBuffer);
            }
            lzwState->dataPtr = pArchive->compBuf;

            /*
             * Next we read as much as we can.
             */
            if (kNuGenCompBufSize - lzwState->dataInBuffer < compRemaining)
                getSize = kNuGenCompBufSize - lzwState->dataInBuffer;
            else
                getSize = compRemaining;

            /*printf("+++ READING %ld\n", getSize);*/
            err = Nu_FRead(infp, lzwState->dataPtr + lzwState->dataInBuffer,
                    getSize);
            if (err != kNuErrNone) {
                Nu_ReportError(NU_BLOB, err,
                    "failed reading compressed data (%u bytes)", getSize);
                goto bail;
            }
            lzwState->dataInBuffer += getSize;
            compRemaining -= getSize;

            Assert(compRemaining < 32767*65536);
            Assert(lzwState->dataInBuffer <= kNuGenCompBufSize);
        }
        Assert(lzwState->dataInBuffer);

        /*
         * Read the LZW block header.
         */
        if (isType2) {
            rleLen = Nu_GetHeaderByte(lzwState);
            rleLen |= Nu_GetHeaderByte(lzwState) << 8;
            lzwUsed = rleLen & 0x8000 ? true : false;
            rleLen &= 0x1fff;
            rleUsed = (rleLen != kNuLZWBlockSize);

            if (lzwUsed) {
                lzwLen = Nu_GetHeaderByte(lzwState);
                lzwLen |= Nu_GetHeaderByte(lzwState) << 8;
                lzwLen -= 4;    /* don't include header bytes */
            }
        } else {
            rleLen = Nu_GetHeaderByte(lzwState);
            rleLen |= Nu_GetHeaderByte(lzwState) << 8;
            lzwUsed = Nu_GetHeaderByte(lzwState);
            if (lzwUsed != 0 && lzwUsed != 1) {
                err = kNuErrBadData;
                Nu_ReportError(NU_BLOB, err, "garbled LZW header");
                goto bail;
            }
            rleUsed = (rleLen != kNuLZWBlockSize);
        }

        /*DBUG_LZW(("### CHUNK rleLen=%d(%d) lzwLen=%d(%d) uncompRem=%ld\n",
            rleLen, rleUsed, lzwLen, lzwUsed, uncompRemaining));*/

        if (uncompRemaining <= kNuLZWBlockSize)
            writeLen = uncompRemaining;     /* last block */
        else
            writeLen = kNuLZWBlockSize;

        #ifndef NDEBUG
        writeBuf = NULL;
        #endif

        /*
         * Decode the chunk, and point "writeBuf" at the uncompressed data.
         *
         * LZW always expands from the read buffer into lzwState->lzwOutBuf.
         * RLE expands from a specific buffer to lzwState->rleOutBuf.
         */
        if (lzwUsed) {
            if (!isType2) {
                err = Nu_ExpandLZW1(lzwState, rleLen);
            } else {
                if (pRecord->isBadMac || pArchive->valIgnoreLZW2Len) {
                    /* might be big-endian, might be okay; just ignore it */
                    lzwLen = (uint32_t) -1;
                } else if (lzwState->dataInBuffer < lzwLen) {
                    /* rare -- GSHK will do this if you don't let it finish */
                    err = kNuErrBufferUnderrun;
                    Nu_ReportError(NU_BLOB, err, "not enough compressed data "
                        "-- archive truncated during creation?");
                    goto bail;
                }
                err = Nu_ExpandLZW2(lzwState, rleLen, lzwLen);
            }

            BailError(err);

            if (rleUsed) {
                err = Nu_ExpandRLE(lzwState, lzwState->lzwOutBuf, rleLen);
                BailError(err);
                writeBuf = lzwState->rleOutBuf;
            } else {
                writeBuf = lzwState->lzwOutBuf;
            }

        } else {
            if (rleUsed) {
                err = Nu_ExpandRLE(lzwState, lzwState->dataPtr, rleLen);
                BailError(err);
                writeBuf = lzwState->rleOutBuf;
                inCount = rleLen;
            } else {
                writeBuf = lzwState->dataPtr;
                inCount = writeLen;
            }
            
            /*
             * Advance the input buffer data pointers to consume the input.
             * The LZW expansion functions do this for us, but we're not
             * using LZW.
             */
            lzwState->dataPtr += inCount;
            lzwState->dataInBuffer -= inCount;
            Assert(lzwState->dataInBuffer < 32767*65536);

            /* no LZW used, reset pointers */
            lzwState->entry = kNuLZWFirstCode;  /* 0x0101 */
            lzwState->resetFix = false;
        }

        Assert(writeBuf != NULL);

        /*
         * Compute the CRC of the uncompressed data, and write it.  For
         * LZW/1, the CRC of the last block includes the zeros that pad
         * it out to 4096 bytes.
         *
         * See commentary in the compression code for why we have to
         * compute two CRCs for LZW/1.
         */
        if (pThreadCrc != NULL) {
            *pThreadCrc = Nu_CalcCRC16(*pThreadCrc, writeBuf, writeLen);
        }
        if (!isType2) {
            lzwState->chunkCrc = Nu_CalcCRC16(lzwState->chunkCrc,
                writeBuf, kNuLZWBlockSize);
        }

        /* write the data, possibly doing an EOL conversion */
        err = Nu_FunnelWrite(pArchive, pFunnel, writeBuf, writeLen);
        if (err != kNuErrNone) {
            if (err != kNuErrAborted)
                Nu_ReportError(NU_BLOB, err, "unable to write output");
            goto bail;
        }

        uncompRemaining -= writeLen;
        Assert(uncompRemaining < 32767*65536);
    }

    /*
     * It appears that ShrinkIt appends an extra byte after the last
     * LZW block.  The byte is included in the compThreadEOF, but isn't
     * consumed by the LZW expansion routine, so it's usually harmless.
     *
     * It is *possible* for extra bytes to be here legitimately, but very
     * unlikely.  The very last block is always padded out to 4K with
     * zeros.  If you found a situation where that last block failed
     * to compress with RLE and LZW (perhaps the last block filled up
     * all but the last 2 or 3 bytes with uncompressible data), but
     * earlier data made the overall file compressible, you would have
     * a few stray bytes in the archive.
     *
     * This is a little easier to do if the last block has lots of single
     * 0xdb characters in it, since that requires RLE to escape them.
     *
     * Whatever the case, issue a warning if it looks like there's too
     * many of them.
     */
    if (lzwState->dataInBuffer > 1) {
        DBUG(("--- Found %ld bytes following compressed data (compRem=%ld)\n",
            lzwState->dataInBuffer, compRemaining));
        if (lzwState->dataInBuffer > 32) {
            Nu_ReportError(NU_BLOB, kNuErrNone, "(Warning) lots of fluff (%u)",
                lzwState->dataInBuffer);
        }
    }

    /*
     * We might be okay with stray bytes in the thread, but we're definitely
     * not okay with anything identified as compressed data being unused.
     */
    if (compRemaining) {
        err = kNuErrBadData;
        Nu_ReportError(NU_BLOB, err,
            "not all compressed data was used (%u/%u)",
            compRemaining, lzwState->dataInBuffer);
        goto bail;
    }

    /*
     * ShrinkIt used to put the CRC in the stream and not in the thread
     * header.  For LZW/1, we check the CRC here; for LZW/2, we hope it's
     * in the thread header.  (As noted in the compression code, it's
     * possible to end up with two CRCs or no CRCs.)
     */
    if (!isType2 && !pArchive->valIgnoreCRC) {
        if (lzwState->chunkCrc != lzwState->fileCrc) {
            if (!Nu_ShouldIgnoreBadCRC(pArchive, pRecord, kNuErrBadDataCRC)) {
                err = kNuErrBadDataCRC;
                Nu_ReportError(NU_BLOB, err,
                    "expected 0x%04x, got 0x%04x (LZW/1)",
                    lzwState->fileCrc, lzwState->chunkCrc);
                (void) Nu_FunnelFlush(pArchive, pFunnel);
                goto bail;
            }
        } else {
            DBUG(("--- LZW/1 CRCs match (0x%04x)\n", lzwState->chunkCrc));
        }
    }

bail:
    return err;
}

#endif /*ENABLE_LZW*/
