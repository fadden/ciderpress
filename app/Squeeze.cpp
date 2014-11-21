/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Implementation of SQueeze (RLE+Huffman) compression.
 *
 * This was ripped fairly directly from Squeeze.c in NufxLib.  Because
 * there's relatively little code, and providing direct access to the
 * compression functions already in NuLib is a little unwieldy, I've just
 * cut & pasted the necessary pieces here.
 */
#include "stdafx.h"
#include "Squeeze.h"
#include "NufxArchive.h"

#define kSqBufferSize   8192        /* must hold full SQ header, and % 128 */

#define kNuSQMagic      0xff76      /* magic value for file header */
#define kNuSQRLEDelim   0x90        /* RLE delimiter */
#define kNuSQEOFToken   256         /* distinguished stop symbol */
#define kNuSQNumVals    257         /* 256 symbols + stop */


/*
 * ===========================================================================
 *      Unsqueeze
 * ===========================================================================
 */

/*
 * State during uncompression.
 */
typedef struct USQState {
    unsigned long   dataInBuffer;
    unsigned char*  dataPtr;
    int             bitPosn;
    int             bits;

    /*
     * Decoding tree; first "nodeCount" values are populated.  Positive
     * values are indices to another node in the tree, negative values
     * are literals (+1 because "negative zero" doesn't work well).
     */
    int             nodeCount;
    struct {
        short       child[2];       /* left/right kids, must be signed 16-bit */
    } decTree[kNuSQNumVals-1];
} USQState;


/*
 * Decode the next symbol from the Huffman stream.
 */
static NuError USQDecodeHuffSymbol(USQState* pUsqState, int* pVal)
{
    short val = 0;
    int bits, bitPosn;

    bits = pUsqState->bits;     /* local copy */
    bitPosn = pUsqState->bitPosn;

    do {
        if (++bitPosn > 7) {
            /* grab the next byte and use that */
            bits = *pUsqState->dataPtr++;
            bitPosn = 0;
            if (!pUsqState->dataInBuffer--)
                return kNuErrBufferUnderrun;

            val = pUsqState->decTree[val].child[1 & bits];
        } else {
            /* still got bits; shift right and use it */
            val = pUsqState->decTree[val].child[1 & (bits >>= 1)];
        }
    } while (val >= 0);

    /* val is negative literal; add one to make it zero-based then negate it */
    *pVal = -(val + 1);

    pUsqState->bits = bits;
    pUsqState->bitPosn = bitPosn;

    return kNuErrNone;
}


/*
 * Read two bytes of signed data out of the buffer.
 */
static inline NuError USQReadShort(USQState* pUsqState, short* pShort)
{
    if (pUsqState->dataInBuffer < 2)
        return kNuErrBufferUnderrun;

    *pShort = *pUsqState->dataPtr++;
    *pShort |= (*pUsqState->dataPtr++) << 8;
    pUsqState->dataInBuffer -= 2;

    return kNuErrNone;
}

/*
 * Wrapper for fread().  Note the arguments resemble read(2) rather
 * than fread(3S).
 */
static NuError SQRead(FILE* fp, void* buf, size_t nbyte)
{
    size_t result;

    ASSERT(buf != NULL);
    ASSERT(nbyte > 0);
    ASSERT(fp != NULL);

    errno = 0;
    result = fread(buf, 1, nbyte, fp);
    if (result != nbyte)
        return errno ? (NuError)errno : kNuErrFileRead;
    return kNuErrNone;
}

NuError UnSqueeze(FILE* fp, unsigned long realEOF, ExpandBuffer* outExp,
          bool fullSqHeader, int blockSize)
{
    /*
     * Because we have a stop symbol, knowing the uncompressed length of
     * the file is not essential.
     */

    NuError err = kNuErrNone;
    USQState usqState;
    unsigned long compRemaining, getSize;
    unsigned short magic, fileChecksum, checksum;   // fullSqHeader only
    short nodeCount;
    int i, inrep;
    unsigned char* tmpBuf = NULL;
    unsigned char lastc = 0;

    tmpBuf = (unsigned char*) malloc(kSqBufferSize);
    if (tmpBuf == NULL) {
        err = kNuErrMalloc;
        goto bail;
    }

    usqState.dataInBuffer = 0;
    usqState.dataPtr = tmpBuf;

    compRemaining = realEOF;
    if ((fullSqHeader && compRemaining < 8) ||
        (!fullSqHeader && compRemaining < 3))
    {
        err = kNuErrBadData;
        LOGI("too short to be valid SQ data");
        goto bail;
    }

    /*
     * Round up to the nearest 128-byte boundary.  We need to read
     * everything out of the file in case this is a streaming archive.
     * Because the compressed data has an embedded stop symbol, it's okay
     * to "overrun" the expansion code.
     */
    if (blockSize != 0) {
        compRemaining =
            ((compRemaining + blockSize-1) / blockSize) * blockSize;
    }

    /* want to grab up to kSqBufferSize bytes */
    if (compRemaining > kSqBufferSize)
        getSize = kSqBufferSize;
    else
        getSize = compRemaining;

    /*
     * Grab a big chunk.  "compRemaining" is the amount of compressed
     * data left in the file, usqState.dataInBuffer is the amount of
     * compressed data left in the buffer.
     *
     * For BNY, we want to read 128-byte blocks.
     */
    if (getSize) {
        ASSERT(getSize <= kSqBufferSize);
        err = SQRead(fp, usqState.dataPtr, getSize);
        if (err != kNuErrNone) {
            LOGI("failed reading compressed data (%ld bytes)", getSize);
            goto bail;
        }
        usqState.dataInBuffer += getSize;
        if (getSize > compRemaining)
            compRemaining = 0;
        else
            compRemaining -= getSize;
    }

    /* reset dataPtr */
    usqState.dataPtr = tmpBuf;

    /*
     * Read the header.  We assume that the header will fit in the
     * compression buffer ( sq allowed 300+ for the filename, plus
     * 257*2 for the tree, plus misc).
     */
    ASSERT(kSqBufferSize > 1200);
    if (fullSqHeader) {
        err = USQReadShort(&usqState, (short*)&magic);
        if (err != kNuErrNone)
            goto bail;
        if (magic != kNuSQMagic) {
            err = kNuErrBadData;
            LOGI("bad magic number in SQ block");
            goto bail;
        }

        err = USQReadShort(&usqState, (short*)&fileChecksum);
        if (err != kNuErrNone)
            goto bail;

        checksum = 0;

        /* skip over the filename */
        while (*usqState.dataPtr++ != '\0')
            usqState.dataInBuffer--;
        usqState.dataInBuffer--;
    }

    err = USQReadShort(&usqState, &nodeCount);
    if (err != kNuErrNone)
        goto bail;
    if (nodeCount < 0 || nodeCount >= kNuSQNumVals) {
        err = kNuErrBadData;
        LOGI("invalid decode tree in SQ (%d nodes)", nodeCount);
        goto bail;
    }
    usqState.nodeCount = nodeCount;

    /* initialize for possibly empty tree (only happens on an empty file) */
    usqState.decTree[0].child[0] = -(kNuSQEOFToken+1);
    usqState.decTree[0].child[1] = -(kNuSQEOFToken+1);

    /* read the nodes, ignoring "read errors" until we're done */
    for (i = 0; i < nodeCount; i++) {
        err = USQReadShort(&usqState, &usqState.decTree[i].child[0]);
        err = USQReadShort(&usqState, &usqState.decTree[i].child[1]);
    }
    if (err != kNuErrNone) {
        err = kNuErrBadData;
        LOGI("SQ data looks truncated at tree");
        goto bail;
    }

    usqState.bitPosn = 99;      /* force an immediate read */

    /*
     * Start pulling data out of the file.  We have to Huffman-decode
     * the input, and then feed that into an RLE expander.
     *
     * A completely lopsided (and broken) Huffman tree could require
     * 256 tree descents, so we want to try to ensure we have at least 256
     * bits in the buffer.  Otherwise, we could get a false buffer underrun
     * indication back from DecodeHuffSymbol.
     *
     * The SQ sources actually guarantee that a code will fit entirely
     * in 16 bits, but there's no reason not to use the larger value.
     */
    inrep = false;
    while (1) {
        int val;

        if (usqState.dataInBuffer < 65 && compRemaining) {
            /*
             * Less than 256 bits, but there's more in the file.
             *
             * First thing we do is slide the old data to the start of
             * the buffer.
             */
            if (usqState.dataInBuffer) {
                ASSERT(tmpBuf != usqState.dataPtr);
                memmove(tmpBuf, usqState.dataPtr, usqState.dataInBuffer);
            }
            usqState.dataPtr = tmpBuf;

            /*
             * Next we read as much as we can.
             */
            if (kSqBufferSize - usqState.dataInBuffer < compRemaining)
                getSize = kSqBufferSize - usqState.dataInBuffer;
            else
                getSize = compRemaining;

            ASSERT(getSize <= kSqBufferSize);
            //LOGI("Reading from offset=%ld (compRem=%ld)",
            //  ftell(fp), compRemaining);
            err = SQRead(fp, usqState.dataPtr + usqState.dataInBuffer,
                        getSize);
            if (err != kNuErrNone) {
                LOGI("failed reading compressed data (%ld bytes, err=%d)",
                    getSize, err);
                goto bail;
            }
            usqState.dataInBuffer += getSize;
            if (getSize > compRemaining)
                compRemaining = 0;
            else
                compRemaining -= getSize;

            ASSERT(compRemaining < 32767*65536);
            ASSERT(usqState.dataInBuffer <= kSqBufferSize);
        }

        err = USQDecodeHuffSymbol(&usqState, &val);
        if (err != kNuErrNone) {
            LOGI("failed decoding huff symbol");
            goto bail;
        }

        if (val == kNuSQEOFToken)
            break;

        /*
         * Feed the symbol into the RLE decoder.
         */
        if (inrep) {
            /*
             * Last char was RLE delim, handle this specially.  We use
             * --val instead of val-- because we already emitted the
             * first occurrence of the char (right before the RLE delim).
             */
            if (val == 0) {
                /* special case -- just an escaped RLE delim */
                lastc = kNuSQRLEDelim;
                val = 2;
            }
            while (--val) {
                /*if (pCrc != NULL)
                    *pCrc = Nu_CalcCRC16(*pCrc, &lastc, 1);*/
                if (outExp != NULL)
                    outExp->Putc(lastc);
                if (fullSqHeader) {
                    checksum += lastc;
                }
            }
            inrep = false;
        } else {
            /* last char was ordinary */
            if (val == kNuSQRLEDelim) {
                /* set a flag and catch the count the next time around */
                inrep = true;
            } else {
                lastc = val;
                /*if (pCrc != NULL)
                    *pCrc = Nu_CalcCRC16(*pCrc, &lastc, 1);*/
                if (outExp != NULL)
                    outExp->Putc(lastc);
                if (fullSqHeader) {
                    checksum += lastc;
                }
            }
        }

    }

    if (inrep) {
        err = kNuErrBadData;
        LOGI("got stop symbol when run length expected");
        goto bail;
    }

    if (fullSqHeader) {
        /* verify the checksum stored in the SQ file */
        if (checksum != fileChecksum) {
            err = kNuErrBadDataCRC;
            LOGI("expected 0x%04x, got 0x%04x (SQ)", fileChecksum, checksum);
            goto bail;
        } else {
            LOGI("--- SQ checksums match (0x%04x)", checksum);
        }
    }

    /*
     * Gobble up any unused bytes in the last 128-byte block.  There
     * shouldn't be more than that left over.
     */
    if (compRemaining > kSqBufferSize) {
        err = kNuErrBadData;
        LOGI("wow: found %ld bytes left over", compRemaining);
        goto bail;
    }
    if (compRemaining) {
        LOGI("+++ slurping up last %ld bytes", compRemaining);
        err = SQRead(fp, tmpBuf, compRemaining);
        if (err != kNuErrNone) {
            LOGI("failed reading leftovers");
            goto bail;
        }
    }

bail:
    //if (outfp != NULL)
    //  fflush(outfp);
    free(tmpBuf);
    return err;
}
