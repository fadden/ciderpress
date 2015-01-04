/*
 * NuFX archive manipulation library
 * Copyright (C) 2000-2007 by Andy McFadden, All Rights Reserved.
 * This is free software; you can redistribute it and/or modify it under the
 * terms of the BSD License, see the file COPYING-LIB.
 *
 * Huffman/RLE "squeeze" compression, based on SQ/USQ.  This format is
 * listed in the NuFX documentation, but to my knowledge has never
 * actually been used (until now).  Neither P8 ShrinkIt v3.4 nor II Unshrink
 * handle the format correctly, so this is really only useful as an
 * experiment.
 *
 * The algorithm appears to date back to the CP/M days.  This implementation
 * is based on "xsq"/"xusq" v1.7u by Richard Greenlaw (from December 1982).
 * The code was also present in ARC v5.x.
 *
 * The "nusq.c" implementation found in NuLib was by Marcel J.E. Mol,
 * who got it from Don Elton's sq3/usq2 programs for the Apple II.
 *
 * The SQ file format begins with this:
 *  +00  magic number (0xff76)
 *  +02  checksum on uncompressed data
 *  +04  filename, ending with \0
 * The NuFX format skips the above, starting immediately after it:
 *  +00  node count
 *  +02  node value array [node count], two bytes each
 *  +xx  data immediately follows array
 *
 * NuFX drops the magic number, checksum, and filename from the header,
 * since (with v3 records) all three are redundant.  You can enable this
 * if you want to experiment with SQ-compatible output.
 */
#include "NufxLibPriv.h"

#ifdef ENABLE_SQ

/* if this is defined, create and unpack the full SQ header (debugging only) */
/* #define FULL_SQ_HEADER */


#define kNuSQMagic      0xff76      /* magic value for file header */
#define kNuSQRLEDelim   0x90        /* RLE delimiter */
#define kNuSQEOFToken   256         /* distinguished stop symbol */
#define kNuSQNumVals    257         /* 256 symbols + stop */


/*
 * ===========================================================================
 *      Compression
 * ===========================================================================
 */

#define kNuSQNoChild    (-1)        /* indicates end of path through tree */
#define kNuSQNumNodes   (kNuSQNumVals + kNuSQNumVals -1)
#define kNuSQMaxCount   65535       /* max value you can store in 16 bits */

/* states for the RLE encoding */
typedef enum {
    kNuSQRLEStateUnknown = 0,

    kNuSQRLEStateNoHist,            /* nothing yet */
    kNuSQRLEStateSentChar,          /* lastchar set, no lookahead yet */
    kNuSQRLEStateSendNewC,          /* found run of two, send 2nd w/o DLE */
    kNuSQRLEStateSendCnt,           /* newchar set, DLE sent, send count next */
} NuSQRLEState;

/* nodes in the Huffman encoding tree */
typedef struct EncTreeNode {
    int     weight;             /* #of appearances */
    int     tdepth;             /* length on longest path in tree */
    int     lchild, rchild;     /* indexes to next level */
} EncTreeNode;

/*
 * State during compression.
 */
typedef struct SQState {
    NuArchive*      pArchive;
    int             doCalcCRC;  /* boolean; if set, compute CRC on input */
    uint16_t        crc;

    NuStraw*        pStraw;
    long            uncompRemaining;

    #ifdef FULL_SQ_HEADER
    uint16_t        checksum;
    #endif

    /*
     * RLE state stuff.
     */
    NuSQRLEState    rleState;
    int             lastSym;
    int             likeCount;

    /*
     * Huffman state stuff.
     */
    EncTreeNode node[kNuSQNumNodes];

    int         treeHead;           /* index to head node of final tree */

    /* encoding table */
    int     codeLen[kNuSQNumVals];  /* number of bits in code for symbol N */
    uint16_t    code[kNuSQNumVals]; /* bits for symbol N (first bit in lsb) */
    uint16_t    tmpCode;            /* temporary code value */
} SQState;


/*
 * Get the next byte from the input straw.  Also updates the checksum
 * and SQ CRC, if "doCalcCRC" is set to true.
 *
 * This isn't exactly fast, but then this isn't exactly a fast algorithm,
 * and there's not much point in optimizing something that isn't going
 * to get used much.
 *
 * Returns kNuSQEOFToken as the value when we're out of data.
 */
static NuError Nu_SQGetcCRC(SQState* pSqState, int* pSym)
{
    NuError err;
    uint8_t c;

    if (!pSqState->uncompRemaining) {
        *pSym = kNuSQEOFToken;
        return kNuErrNone;
    }

    err = Nu_StrawRead(pSqState->pArchive, pSqState->pStraw, &c, 1);
    if (err == kNuErrNone) {
        if (pSqState->doCalcCRC) {
            #ifdef FULL_SQ_HEADER
            pSqState->checksum += c;
            #endif
            pSqState->crc = Nu_CalcCRC16(pSqState->crc, &c, 1);
        }
        *pSym = c;
        pSqState->uncompRemaining--;
    }

    return err;
}

/*
 * Get the next byte from the post-RLE input stream.
 *
 * Returns kNuSQEOFToken in "*pSum" when we reach the end of the input.
 */
static NuError Nu_SQGetcRLE(SQState* pSqState, int* pSym)
{
    NuError err = kNuErrNone;
    int likeCount, newSym;

    switch (pSqState->rleState) {
    case kNuSQRLEStateNoHist:
        /* No relevant history */
        pSqState->rleState = kNuSQRLEStateSentChar;
        err = Nu_SQGetcCRC(pSqState, pSym);
        pSqState->lastSym = *pSym;
        break;

    case kNuSQRLEStateSentChar:
        /* lastChar is set, need lookahead */
        switch (pSqState->lastSym) {
        case kNuSQRLEDelim:
            /* send all DLEs escaped; note this is horrible for a run of DLEs */
            pSqState->rleState = kNuSQRLEStateNoHist;
            *pSym = 0;      /* zero len is how we define an escaped DLE */
            break;
        case kNuSQEOFToken:
            *pSym = kNuSQEOFToken;
            break;
        default:
            /*
             * Try for a run, using the character we previous read as
             * the base.  Thus, if the next character we read matches,
             * we have a run of two.  The count describes the total
             * length of the run, including the character we've already
             * emitted.
             */
            likeCount = 0;
            do {
                likeCount++;
                err = Nu_SQGetcCRC(pSqState, &newSym);
                if (err != kNuErrNone)
                    goto bail;
            } while (newSym == pSqState->lastSym && likeCount < 255);

            switch (likeCount) {
            case 1:
                /* not a run, return first one we got */
                pSqState->lastSym = newSym;
                *pSym = newSym;
                break;
            case 2:
                /* not long enough for run; return second one next time thru */
                pSqState->rleState = kNuSQRLEStateSendNewC;
                *pSym = pSqState->lastSym;      /* 1st new one */
                pSqState->lastSym = newSym;     /* 2nd new one */
                break;
            default:
                pSqState->rleState = kNuSQRLEStateSendCnt;
                pSqState->likeCount = likeCount;
                pSqState->lastSym = newSym;     /* 1st one after the run */
                *pSym = kNuSQRLEDelim;
                break;
            }
        }
        break;

    case kNuSQRLEStateSendNewC:
        /* send first char past a run of two */
        pSqState->rleState = kNuSQRLEStateSentChar;
        *pSym = pSqState->lastSym;
        break;

    case kNuSQRLEStateSendCnt:
        /* Sent DLE for repeat sequence, send count */
        pSqState->rleState = kNuSQRLEStateSendNewC;
        *pSym = pSqState->likeCount;
        break;

    default:
        {
            NuArchive* pArchive = pSqState->pArchive;

            err = kNuErrInternal;
            Nu_ReportError(NU_BLOB, err, "invalid state %d in SQ RLE encode",
                pSqState->rleState);
            break;
        }
    }

bail:
    return err;
}


/*
 * Comment from xsq.c:
 *
 *  This translation uses the Huffman algorithm to develop a
 *  binary tree representing the decoding information for
 *  a variable length bit string code for each input value.
 *  Each string's length is in inverse proportion to its
 *  frequency of appearance in the incoming data stream.
 *  The encoding table is derived from the decoding table.
 *
 *  The range of valid values into the Huffman algorithm are
 *  the values of a byte stored in an integer plus the special
 *  endfile value chosen to be an adjacent value. Overall, 0-SPEOF.
 *
 *  The "node" array of structures contains the nodes of the
 *  binary tree. The first NUMVALS nodes are the leaves of the
 *  tree and represent the values of the data bytes being
 *  encoded and the special endfile, SPEOF.
 *  The remaining nodes become the internal nodes of the tree.
 *
 *  In the original design it was believed that
 *  a Huffman code would fit in the same number of
 *  bits that will hold the sum of all the counts.
 *  That was disproven by a user's file and was a rare but
 *  infamous bug. This version attempts to choose among equally
 *  weighted subtrees according to their maximum depths to avoid
 *  unnecessarily long codes. In case that is not sufficient
 *  to guarantee codes <= 16 bits long, we initially scale
 *  the counts so the total fits in an unsigned integer, but
 *  if codes longer than 16 bits are generated the counts are
 *  rescaled to a lower ceiling and code generation is retried.
 */

/*
 * Return the greater of two integers.
 */
static int Nu_SQMax(int a, int b)
{
    if (a > b)
        return a;
    else
        return b;
}

/*
 * Compare two trees, if a > b return true, else return false.
 * Priority is given to weight, then depth.  "a" and "b" are heaps,
 * so we only need to look at the root element.
 */
static int Nu_SQCmpTrees(SQState* pSqState, int a, int b)
{
    if (pSqState->node[a].weight > pSqState->node[b].weight)
        return true;
    if (pSqState->node[a].weight == pSqState->node[b].weight)
        if (pSqState->node[a].tdepth > pSqState->node[b].tdepth)
            return true;
    return false;
}

/*
 *  heap() and adjust() maintain a list of binary trees as a
 *  heap with the top indexing the binary tree on the list
 *  which has the least weight or, in case of equal weights,
 *  least depth in its longest path. The depth part is not
 *  strictly necessary, but tends to avoid long codes which
 *  might provoke rescaling.
 */

/*
 * Recursively make a heap from a heap with a new top.
 */
static void Nu_SQHeapAdjust(SQState* pSqState, int list[], int top, int bottom)
{
    int k, temp;

    k = 2 * top + 1;    /* left child of top */
    temp = list[top];   /* remember root node of top tree */
    if (k <= bottom) {
        if (k < bottom && Nu_SQCmpTrees(pSqState, list[k], list[k + 1]))
            k++;

        /* k indexes "smaller" child (in heap of trees) of top */
        /* now make top index "smaller" of old top and smallest child */
        if (Nu_SQCmpTrees(pSqState, temp, list[k])) {
            list[top] = list[k];
            list[k] = temp;
            /* Make the changed list a heap */
            Nu_SQHeapAdjust(pSqState, list, k, bottom); /*recursive*/
        }
    }
}

/*
 * Create a heap.
 */
static void Nu_SQHeap(SQState* pSqState, int list[], int length)
{
    int i;

    for (i = (length - 2) / 2; i >= 0; i--)
        Nu_SQHeapAdjust(pSqState, list, i, length - 1);
}


/*
 * Build the encoding tree.
 *
 *  HUFFMAN ALGORITHM: develops the single element trees
 *  into a single binary tree by forming subtrees rooted in
 *  interior nodes having weights equal to the sum of weights of all
 *  their descendents and having depth counts indicating the
 *  depth of their longest paths.
 *
 *  When all trees have been formed into a single tree satisfying
 *  the heap property (on weight, with depth as a tie breaker)
 *  then the binary code assigned to a leaf (value to be encoded)
 *  is then the series of left (0) and right (1)
 *  paths leading from the root to the leaf.
 *  Note that trees are removed from the heaped list by
 *  moving the last element over the top element and
 *  reheaping the shorter list.
 */
static void Nu_SQBuildTree(SQState* pSqState, int list[], int len)
{
    int freenode;           /* next free node in tree */
    EncTreeNode* frnp;      /* free node pointer */
    int lch, rch;           /* temporaries for left, right children */

    /*
     * Initialize index to next available (non-leaf) node.
     * Lower numbered nodes correspond to leaves (data values).
     */
    freenode = kNuSQNumVals;

    while (len > 1) {
        /*
         * Take from list two btrees with least weight
         * and build an interior node pointing to them.
         * This forms a new tree.
         */
        lch = list[0];  /* This one will be left child */

        /* delete top (least) tree from the list of trees */
        list[0] = list[--len];
        Nu_SQHeapAdjust(pSqState, list, 0, len - 1);

        /* Take new top (least) tree. Reuse list slot later */
        rch = list[0];  /* This one will be right child */

        /*
         * Form new tree from the two least trees using
         * a free node as root. Put the new tree in the list.
         */
        frnp = &pSqState->node[freenode]; /* address of next free node */
        list[0] = freenode++;   /* put at top for now */
        frnp->lchild = lch;
        frnp->rchild = rch;
        frnp->weight =
            pSqState->node[lch].weight + pSqState->node[rch].weight;
        frnp->tdepth = 1 + Nu_SQMax(pSqState->node[lch].tdepth,
                                    pSqState->node[rch].tdepth);

        /* reheap list to get least tree at top*/
        Nu_SQHeapAdjust(pSqState, list, 0, len - 1);
    }

    pSqState->treeHead = list[0]; /* head of final tree */
}


/*
 * Recursive routine to walk the indicated subtree and level
 * and maintain the current path code in bstree. When a leaf
 * is found the entire code string and length are put into
 * the encoding table entry for the leaf's data value .
 *
 * Returns zero on success, nonzero if codes are too long.
 */
static int Nu_SQBuildEncTable(SQState* pSqState, int level, int root)
{
    int l, r;

    l = pSqState->node[root].lchild;
    r = pSqState->node[root].rchild;

    if (l == kNuSQNoChild && r == kNuSQNoChild) {
        /* Leaf. Previous path determines bit string
         * code of length level (bits 0 to level - 1).
         * Ensures unused code bits are zero.
         */
        pSqState->codeLen[root] = level;
        pSqState->code[root] =
            pSqState->tmpCode & (((uint16_t)~0) >> (16 - level));
        return (level > 16) ? -1 : 0;
    } else {
        if (l != kNuSQNoChild) {
            /* Clear path bit and continue deeper */
            pSqState->tmpCode &= ~(1 << level);
            /* NOTE RECURSION */
            if (Nu_SQBuildEncTable(pSqState, level + 1, l) != 0)
                return -1;
        }
        if (r != kNuSQNoChild) {
            /* Set path bit and continue deeper */
            pSqState->tmpCode |= 1 << level;
            /* NOTE RECURSION */
            if (Nu_SQBuildEncTable(pSqState, level + 1, r) != 0)
                return -1;
        }
    }

    return 0;       /* if we got here we're ok so far */
}


/*
 *  The count of number of occurrances of each input value
 *  have already been prevented from exceeding MAXCOUNT.
 *  Now we must scale them so that their sum doesn't exceed
 *  ceiling and yet no non-zero count can become zero.
 *  This scaling prevents errors in the weights of the
 *  interior nodes of the Huffman tree and also ensures that
 *  the codes will fit in an unsigned integer. Rescaling is
 *  used if necessary to limit the code length.
 */
static void Nu_SQScale(SQState* pSqState, int ceiling)
{
    int i;
    int wt, ovflw, divisor;
    uint16_t sum;
    int increased;     /* flag */

    do {
        for (i = sum = ovflw = 0; i < kNuSQNumVals; i++) {
            if (pSqState->node[i].weight > (ceiling - sum))
                ovflw++;
            sum += pSqState->node[i].weight;
        }

        divisor = ovflw + 1;    /* use the high 16 bits of the sum */

        /* Ensure no non-zero values are lost */
        increased = false;
        for (i = 0; i < kNuSQNumVals; i++) {
            wt = pSqState->node[i].weight;
            if (wt < divisor && wt != 0) {
                /* Don't fail to provide a code if it's used at all */
                pSqState->node[i].weight = divisor;
                increased = true;
            }
        }
    } while(increased);

    /* scaling factor choosen and minimums are set; now do the downscale */
    if (divisor > 1) {
        for (i = 0; i < kNuSQNumVals; i++)
            pSqState->node[i].weight /= divisor;
    }
}

/*
 * Build a frequency table from the post-RLE input stream, then generate
 * an encoding tree from the results.
 */
static NuError Nu_SQComputeHuffTree(SQState* pSqState)
{
    NuError err = kNuErrNone;
    int btreeList[kNuSQNumVals];        /* list of intermediate binary trees */
    int listLen;                        /* length of btreeList */
    int ceiling;                        /* limit for scaling */
    int i, sym, result;

    /* init tree */
    for (i = 0; i < kNuSQNumNodes; i++) {
        pSqState->node[i].weight = 0;
        pSqState->node[i].tdepth = 0;
        pSqState->node[i].lchild = kNuSQNoChild;
        pSqState->node[i].rchild = kNuSQNoChild;
    }

    DBUG(("+++ SQ scanning...\n"));

    do {
        int* pWeight;

        err = Nu_SQGetcRLE(pSqState, &sym);
        if (err != kNuErrNone)
            goto bail;

        Assert(sym >= 0 && sym <= kNuSQEOFToken);
        pWeight = &pSqState->node[(unsigned)sym].weight;
        if (*pWeight != kNuSQMaxCount)
            (*pWeight)++;
    } while (sym != kNuSQEOFToken);

    DBUG(("+++ SQ generating tree...\n"));

    ceiling = kNuSQMaxCount;

    do {
        if (ceiling != kNuSQMaxCount) {
            DBUG(("+++ SQ rescaling\n"));
        }

        /* pick a divisor and scale everything to fit in "ceiling" */
        Nu_SQScale(pSqState, ceiling);

        ceiling /= 2;       /* in case we need to rescale */

        /*
         * Build list of single node binary trees having
         * leaves for the input values with non-zero counts
         */
        for (i = listLen = 0; i < kNuSQNumVals; i++) {
            if (pSqState->node[i].weight != 0) {
                pSqState->node[i].tdepth = 0;
                btreeList[listLen++] = i;
            }
        }

        /*
         * Arrange list of trees into a heap with the entry
         * indexing the node with the least weight a the top.
         */
        Nu_SQHeap(pSqState, btreeList, listLen);

        /* convert the list of trees to a single decoding tree */
        Nu_SQBuildTree(pSqState, btreeList, listLen);

        /* initialize encoding table */
        for (i = 0; i < kNuSQNumVals; i++)
            pSqState->codeLen[i] = 0;

        /*
         * Recursively build the encoding table; returns non-zero (failure)
         * if any code is > 16 bits long.
         */
        result = Nu_SQBuildEncTable(pSqState, 0, pSqState->treeHead);
    } while (result != 0);

#if 0
{
    int jj;
    printf("init_huff\n");
    for (jj = 0; jj < kNuSQNumNodes; jj++) {
        printf("NODE %d: w=%d d=%d l=%d r=%d\n", jj,
            pSqState->node[jj].weight,
            pSqState->node[jj].tdepth,
            pSqState->node[jj].lchild,
            pSqState->node[jj].rchild);
    }
}
#endif

bail:
    return err;
}


/*
 * Compress data from input to output, using the values in the "code"
 * and "codeLen" arrays.
 */
static NuError Nu_SQCompressInput(SQState* pSqState, FILE* fp,
    long* pCompressedLen)
{
    NuError err = kNuErrNone;
    int sym = kNuSQEOFToken-1;
    uint32_t bits, code;       /* must hold at least 23 bits */
    int codeLen, gotbits;
    long compressedLen;
    
    DBUG(("+++ SQ compressing\n"));

    Assert(sizeof(bits) >= 4);
    compressedLen = *pCompressedLen;

    bits = 0;
    gotbits = 0;
    while (sym != kNuSQEOFToken) {
        err = Nu_SQGetcRLE(pSqState, &sym);
        if (err != kNuErrNone)
            goto bail;

        code = pSqState->code[sym];
        codeLen = pSqState->codeLen[sym];

        bits |= code << gotbits;
        gotbits += codeLen;

        /* if we have more than a byte, output it */
        while (gotbits > 7) {
            putc(bits & 0xff, fp);
            compressedLen++;
            bits >>= 8;
            gotbits -= 8;
        }
    }

    if (gotbits) {
        Assert(gotbits < 8);
        putc(bits & 0xff, fp);
        compressedLen++;
    }

bail:
    *pCompressedLen = compressedLen;
    return err;
}


/*
 * Write a 16-bit value in little-endian order.
 */
static NuError Nu_SQWriteShort(FILE* outfp, short val)
{
    NuError err;
    uint8_t tmpc;

    tmpc = val & 0xff;
    err = Nu_FWrite(outfp, &tmpc, 1);
    if (err != kNuErrNone)
        goto bail;
    tmpc = (val >> 8) & 0xff;
    err = Nu_FWrite(outfp, &tmpc, 1);
    if (err != kNuErrNone)
        goto bail;

bail:
    return err;
}

/*
 * Compress "srcLen" bytes into SQ format, from "pStraw" to "fp".
 *
 * This requires two passes through the input.
 *
 * Bit of trivia: "sq3" on the Apple II self-destructs if you hand
 * it an empty file.  "xsq" works fine, creating an empty tree that
 * "xusq" unpacks.
 */
NuError Nu_CompressHuffmanSQ(NuArchive* pArchive, NuStraw* pStraw, FILE* fp,
    uint32_t srcLen, uint32_t* pDstLen, uint16_t* pCrc)
{
    NuError err = kNuErrNone;
    SQState sqState;
    long compressedLen;
    int i, j, numNodes;

    err = Nu_AllocCompressionBufferIFN(pArchive);
    if (err != kNuErrNone)
        return err;

    sqState.pArchive = pArchive;
    sqState.crc = 0;
    if (pCrc == NULL) {
        sqState.doCalcCRC = false;
    } else {
        sqState.doCalcCRC = true;
        sqState.crc = *pCrc;
    }

    #ifdef FULL_SQ_HEADER
    sqState.checksum = 0;
    #endif

    /*
     * Pass 1: analysis.  Perform a frequency analysis on the post-RLE
     * input file.  This will calculate the file CRCs as a side effect.
     */
    sqState.rleState = kNuSQRLEStateNoHist;
    sqState.uncompRemaining = srcLen;
    sqState.pStraw = pStraw;
    (void) Nu_StrawSetProgressState(pStraw, kNuProgressAnalyzing);

    err = Nu_SQComputeHuffTree(&sqState);
    BailError(err);

    if (pCrc != NULL)
        *pCrc = sqState.crc;

    /*
     * Pass 2: compression.  Using the encoding tree we computed,
     * compress the input with RLE and Huffman.  Start by writing
     * the file header and rewinding the input file.
     */
    sqState.doCalcCRC = false;        /* don't need to re-compute */
    sqState.rleState = kNuSQRLEStateNoHist;     /* reset */
    compressedLen = 0;

    /* rewind for next pass */
    (void) Nu_StrawSetProgressState(pStraw, kNuProgressCompressing);
    err = Nu_StrawRewind(pArchive, pStraw);
    BailError(err);
    sqState.uncompRemaining = srcLen;

    #ifdef FULL_SQ_HEADER
    /* write file header */
    err = Nu_SQWriteShort(fp, kNuSQMagic);
    BailError(err);
    compressedLen += 2;

    err = Nu_SQWriteShort(fp, sqState.checksum);
    BailError(err);
    compressedLen += 2;

    {
        static const char fakename[] = "s.qqq";
        err = Nu_FWrite(fp, fakename, sizeof(fakename));
        BailError(err);
        compressedLen += sizeof(fakename);
    }
    #endif

    /*
     * Original description:
     *  Write out a simplified decoding tree. Only the interior
     *  nodes are written. When a child is a leaf index
     *  (representing a data value) it is recoded as
     *  -(index + 1) to distinguish it from interior indexes
     *  which are recoded as positive indexes in the new tree.
     *  Note that this tree will be empty for an empty file.
     */
    if (sqState.treeHead < kNuSQNumVals)
        numNodes = 0;
    else
        numNodes = sqState.treeHead - (kNuSQNumVals - 1);
    err = Nu_SQWriteShort(fp, (short) numNodes);
    BailError(err);
    compressedLen += 2;

    for (i = sqState.treeHead, j = 0; j < numNodes; j++, i--) {
        int l, r;

        l = sqState.node[i].lchild;
        r = sqState.node[i].rchild;
        l = l < kNuSQNumVals ? -(l + 1) : sqState.treeHead - l;
        r = r < kNuSQNumVals ? -(r + 1) : sqState.treeHead - r;
        err = Nu_SQWriteShort(fp, (short) l);
        BailError(err);
        err = Nu_SQWriteShort(fp, (short) r);
        BailError(err);
        compressedLen += 4;

        /*DBUG(("TREE %d: %d %d\n", j, l, r));*/
    }

    /*
     * Convert the input to RLE/Huffman.
     */
    err = Nu_SQCompressInput(&sqState, fp, &compressedLen);
    BailError(err);

    /*
     * Done!
     */
    *pDstLen = compressedLen;

bail:
    return err;
}


/*
 * ===========================================================================
 *      Expansion
 * ===========================================================================
 */

/*
 * State during uncompression.
 */
typedef struct USQState {
    uint32_t        dataInBuffer;
    uint8_t*        dataPtr;
    int             bitPosn;
    int             bits;

    /*
     * Decoding tree; first "nodeCount" values are populated.  Positive
     * values are indicies to another node in the tree, negative values
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
static NuError Nu_USQDecodeHuffSymbol(USQState* pUsqState, int* pVal)
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
static inline NuError Nu_USQReadShort(USQState* pUsqState, short* pShort)
{
    if (pUsqState->dataInBuffer < 2)
        return kNuErrBufferUnderrun;

    *pShort = *pUsqState->dataPtr++;
    *pShort |= (*pUsqState->dataPtr++) << 8;
    pUsqState->dataInBuffer -= 2;

    return kNuErrNone;
}

/*
 * Expand "SQ" format.
 *
 * Because we have a stop symbol, knowing the uncompressed length of
 * the file is not essential.
 */
NuError Nu_ExpandHuffmanSQ(NuArchive* pArchive, const NuRecord* pRecord,
    const NuThread* pThread, FILE* infp, NuFunnel* pFunnel, uint16_t* pCrc)
{
    NuError err = kNuErrNone;
    USQState usqState;
    uint32_t compRemaining, getSize;
#ifdef FULL_SQ_HEADER
    uint16_t magic, fileChecksum, checksum;
#endif
    short nodeCount;
    int i, inrep;
    uint8_t lastc = 0;

    err = Nu_AllocCompressionBufferIFN(pArchive);
    if (err != kNuErrNone)
        return err;
    Assert(pArchive->compBuf != NULL);

    usqState.dataInBuffer = 0;
    usqState.dataPtr = pArchive->compBuf;
    usqState.bits = usqState.bitPosn = 0;

    compRemaining = pThread->thCompThreadEOF;
#ifdef FULL_SQ_HEADER
    if (compRemaining < 8)
#else
    if (compRemaining < 3)
#endif
    {
        err = kNuErrBadData;
        Nu_ReportError(NU_BLOB, err, "thread too short to be valid SQ data");
        goto bail;
    }

    getSize = compRemaining;
    if (getSize > kNuGenCompBufSize)
        getSize = kNuGenCompBufSize;

    /*
     * Grab a big chunk.  "compRemaining" is the amount of compressed
     * data left in the file, usqState.dataInBuffer is the amount of
     * compressed data left in the buffer.
     */
    err = Nu_FRead(infp, usqState.dataPtr, getSize);
    if (err != kNuErrNone) {
        Nu_ReportError(NU_BLOB, err,
            "failed reading compressed data (%u bytes)", getSize);
        goto bail;
    }
    usqState.dataInBuffer += getSize;
    compRemaining -= getSize;

    /*
     * Read the header.  We assume that the header is less than
     * kNuGenCompBufSize bytes, which is pretty fair since the buffer is
     * currently 20x larger than the longest possible header (sq allowed
     * 300+ for the filename, plus 257*2 for the tree, plus misc).
     */
    Assert(kNuGenCompBufSize > 1200);
#ifdef FULL_SQ_HEADER
    err = Nu_USQReadShort(&usqState, &magic);
    BailError(err);
    if (magic != kNuSQMagic) {
        err = kNuErrBadData;
        Nu_ReportError(NU_BLOB, err, "bad magic number in SQ block");
        goto bail;
    }

    err = Nu_USQReadShort(&usqState, &fileChecksum);
    BailError(err);

    checksum = 0;

    while (*usqState.dataPtr++ != '\0')
        usqState.dataInBuffer--;
    usqState.dataInBuffer--;
#endif

    err = Nu_USQReadShort(&usqState, &nodeCount);
    BailError(err);
    if (nodeCount < 0 || nodeCount >= kNuSQNumVals) {
        err = kNuErrBadData;
        Nu_ReportError(NU_BLOB, err, "invalid decode tree in SQ (%d nodes)",
            nodeCount);
        goto bail;
    }
    usqState.nodeCount = nodeCount;

    /* initialize for possibly empty tree (only happens on an empty file) */
    usqState.decTree[0].child[0] = -(kNuSQEOFToken+1);
    usqState.decTree[0].child[1] = -(kNuSQEOFToken+1);

    /* read the nodes, ignoring "read errors" until we're done */
    for (i = 0; i < nodeCount; i++) {
        err = Nu_USQReadShort(&usqState, &usqState.decTree[i].child[0]);
        err = Nu_USQReadShort(&usqState, &usqState.decTree[i].child[1]);
    }
    if (err != kNuErrNone) {
        err = kNuErrBadData;
        Nu_ReportError(NU_BLOB, err, "SQ data looks truncated at tree");
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
                Assert(pArchive->compBuf != usqState.dataPtr);
                memmove(pArchive->compBuf, usqState.dataPtr,
                    usqState.dataInBuffer);
            }
            usqState.dataPtr = pArchive->compBuf;

            /*
             * Next we read as much as we can.
             */
            if (kNuGenCompBufSize - usqState.dataInBuffer < compRemaining)
                getSize = kNuGenCompBufSize - usqState.dataInBuffer;
            else
                getSize = compRemaining;

            err = Nu_FRead(infp, usqState.dataPtr + usqState.dataInBuffer,
                    getSize);
            if (err != kNuErrNone) {
                Nu_ReportError(NU_BLOB, err,
                    "failed reading compressed data (%u bytes)", getSize);
                goto bail;
            }
            usqState.dataInBuffer += getSize;
            compRemaining -= getSize;

            Assert(compRemaining < 32767*65536);
            Assert(usqState.dataInBuffer <= kNuGenCompBufSize);
        }

        err = Nu_USQDecodeHuffSymbol(&usqState, &val);
        if (err != kNuErrNone) {
            Nu_ReportError(NU_BLOB, err, "failed decoding huff symbol");
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
                if (pCrc != NULL)
                    *pCrc = Nu_CalcCRC16(*pCrc, &lastc, 1);
                err = Nu_FunnelWrite(pArchive, pFunnel, &lastc, 1);
                #ifdef FULL_SQ_HEADER
                checksum += lastc;
                #endif
            }
            inrep = false;
        } else {
            /* last char was ordinary */
            if (val == kNuSQRLEDelim) {
                /* set a flag and catch the count the next time around */
                inrep = true;
            } else {
                lastc = val;
                if (pCrc != NULL)
                    *pCrc = Nu_CalcCRC16(*pCrc, &lastc, 1);
                err = Nu_FunnelWrite(pArchive, pFunnel, &lastc, 1);
                #ifdef FULL_SQ_HEADER
                checksum += lastc;
                #endif
            }
        }

    }

    if (inrep) {
        err = kNuErrBadData;
        Nu_ReportError(NU_BLOB, err,
            "got stop symbol when run length expected");
        goto bail;
    }

    #ifdef FULL_SQ_HEADER
    /* verify the checksum stored in the SQ file */
    if (checksum != fileChecksum && !pArchive->valIgnoreCRC) {
        if (!Nu_ShouldIgnoreBadCRC(pArchive, pRecord, kNuErrBadDataCRC)) {
            err = kNuErrBadDataCRC;
            Nu_ReportError(NU_BLOB, err, "expected 0x%04x, got 0x%04x (SQ)",
                fileChecksum, checksum);
            (void) Nu_FunnelFlush(pArchive, pFunnel);
            goto bail;
        }
    } else {
        DBUG(("--- SQ checksums match (0x%04x)\n", checksum));
    }
    #endif

    /*
     * SQ2 adds an extra 0xff to the end, xsq doesn't.  In any event, it
     * appears that having an extra byte at the end is okay.
     */
    if (usqState.dataInBuffer > 1) {
        DBUG(("--- Found %ld bytes following compressed data (compRem=%ld)\n",
            usqState.dataInBuffer, compRemaining));
        Nu_ReportError(NU_BLOB, kNuErrNone, "(Warning) unexpected fluff (%u)",
            usqState.dataInBuffer);
    }

bail:
    return err;
}

#endif /*ENABLE_SQ*/
