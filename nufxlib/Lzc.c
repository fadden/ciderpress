/*
 * NuFX archive manipulation library
 * Copyright (C) 2000-2007 by Andy McFadden, All Rights Reserved.
 * This is free software; you can redistribute it and/or modify it under the
 * terms of the BSD License, see the file COPYING-LIB.
 *
 * This is the LZW implementation found in the UNIX "compress" command,
 * sometimes referred to as "LZC".  GS/ShrinkIt v1.1 can unpack threads
 * in LZC format, P8 ShrinkIt cannot.  The only other application that
 * is known to create LZC threads is the original NuLib.
 *
 * There's a lot of junk in here for the sake of smaller systems (e.g. MSDOS)
 * and pre-ANSI compilers.  For the most part it has been left unchanged.
 * I have done some minor reformatting, and have undone the authors'
 * penchant for assigning variables inside function call statements, but
 * for the most part it is as it was.  (A much cleaner implementation
 * could probably be derived by adapting the NufxLib Lzw.c code...)
 */
#include "NufxLibPriv.h"

#ifdef ENABLE_LZC

/*#define DEBUG_LZC*/

/*
 * Selected definitions from compress.h.
 */
typedef unsigned short CODE;
typedef unsigned char UCHAR;
typedef unsigned int INTCODE;
typedef unsigned int HASH;
typedef int FLAG;

#ifndef FALSE            /* let's get some sense to this */
#define FALSE 0
#define TRUE !FALSE
#endif

#define CONST       const
#ifndef FAR
# define FAR
#endif
#define NULLPTR(type)   ((type FAR *) NULL)
#define ALLOCTYPE   void

#define INITBITS    9
#define MINBITS     12
#define MAXMAXBITS  16
#define MAXBITS     MAXMAXBITS
#define DFLTBITS    MAXBITS

#define UNUSED      ((CODE)0)   /* Indicates hash table value unused    */
#define CLEAR       ((CODE)256) /* Code requesting table to be cleared  */
#define FIRSTFREE   ((CODE)257) /* First free code for token encoding */
#define MAXTOKLEN   512         /* Max chars in token; size of buffer   */
#define OK          kNuErrNone  /* Result codes from functions:         */

#define BIT_MASK    0x1f
#define BLOCK_MASK  0x80

#define CHECK_GAP   10000L      /* ratio check interval, for COMP40 */

static UCHAR gNu_magic_header[] = { 0x1F,0x9D };

/* don't need these */
/*#define SPLIT_HT    1*/
/*#define SPLIT_PFX   1*/
/*#define COMP40      1*/

#define NOMEM       kNuErrMalloc    /*   Ran out of memory                  */
#define TOKTOOBIG   kNuErrBadData   /*   Token longer than MAXTOKLEN chars  */
#define READERR     kNuErrFileRead  /*   I/O error on input                 */
#define WRITEERR    kNuErrFileWrite /*   I/O error on output                */
#define CODEBAD     kNuErrBadData   /*   Infile contained a bad token code  */
#define TABLEBAD    kNuErrInternal  /*   The tables got corrupted (!)       */
#define NOSAVING    kNuErrNone      /*   no saving in file size             */


/*
 * Normally in COMPUSI.UNI.
 */
static inline ALLOCTYPE FAR *
Nu_LZC_emalloc(NuArchive* pArchive, unsigned int x, int y)
{
    return Nu_Malloc(pArchive, x*y);
}
static inline void
Nu_LZC_efree(NuArchive* pArchive, ALLOCTYPE FAR * ptr)
{
    Nu_Free(pArchive, ptr);
}

/*@H************************ < COMPRESS API    > ****************************
*   $@(#) compapi.c,v 4.3d 90/01/18 03:00:00 don Release ^                  *
*                                                                           *
*   compress : compapi.c  <current version of compress algorithm>           *
*                                                                           *
*   port by  : Donald J. Gloistein                                          *
*                                                                           *
*   Source, Documentation, Object Code:                                     *
*   released to Public Domain.  This code is based on code as documented    *
*   below in release notes.                                                 *
*                                                                           *
*---------------------------  Module Description  --------------------------*
*   Contains source code for modified Lempel-Ziv method (LZW) compression   *
*   and decompression.                                                      *
*                                                                           *
*   This code module can be maintained to keep current on releases on the   *
*   Unix system. The command shell and dos modules can remain the same.     *
*                                                                           *
*--------------------------- Implementation Notes --------------------------*
*                                                                           *
*   compiled with : compress.h compress.fns compress.c                      *
*   linked with   : compress.obj compusi.obj                                *
*                                                                           *
*   problems:                                                               *
*                                                                           *
*                                                                           *
*   CAUTION: Uses a number of defines for access and speed. If you change   *
*            anything, make sure about side effects.                        *
*                                                                           *
* Compression:                                                              *
* Algorithm:  use open addressing double hashing (no chaining) on the       *
* prefix code / next character combination.  We do a variant of Knuth's     *
* algorithm D (vol. 3, sec. 6.4) along with G. Knott's relatively-prime     *
* secondary probe.  Here, the modular division first probe is gives way     *
* to a faster exclusive-or manipulation.                                    *
* Also block compression with an adaptive reset was used in original code,  *
* whereby the code table is cleared when the compression ration decreases   *
* but after the table fills.  This was removed from this edition. The table *
* is re-sized at this point when it is filled , and a special CLEAR code is *
* generated for the decompressor. This results in some size difference from *
* straight version 4.0 joe Release. But it is fully compatible in both v4.0 *
* and v4.01                                                                 *
*                                                                           *
* Decompression:                                                            *
* This routine adapts to the codes in the file building the "string" table  *
* on-the-fly; requiring no table to be stored in the compressed file.  The  *
* tables used herein are shared with those of the compress() routine.       *
*                                                                           *
*     Initials ---- Name ---------------------------------                  *
*      DjG          Donald J. Gloistein, current port to MsDos 16 bit       *
*                   Plus many others, see rev.hst file for full list        *
*      LvR          Lyle V. Rains, many thanks for improved implementation  *
*                   of the compression and decompression routines.          *
*************************************************************************@H*/

#include <stdio.h>

/*
 * LZC state, largely variables with non-local scope.
 */
typedef struct LZCState {
    NuArchive* pArchive;
    int doCalcCRC;
    ushort crc;

    /* compression */
    NuStraw* pStraw;
    FILE* outfp;
    long uncompRemaining;

    /* expansion */
    FILE* infp;
    NuFunnel* pFunnel;
    ushort* pCrc;
    long compRemaining;


    /*
     * Globals from Compress sources.
     */
    int offset;
    long int in_count ;         /* length of input */
    long int bytes_out;         /* length of compressed output */

    INTCODE prefxcode, nextfree;
    INTCODE highcode;
    INTCODE maxcode;
    HASH hashsize;
    int  bits;

    char FAR *sfx;

    #if (SPLIT_PFX)
        CODE FAR *pfx[2];
    #else
        CODE FAR *pfx;
    #endif

    #if (SPLIT_HT)
        CODE FAR *ht[2];
    #else
        CODE FAR *ht;
    #endif

    #ifdef COMP40
    long int ratio;
    long checkpoint;            /* initialized to CHECK_GAP */
    #endif

    #ifdef DEBUG_LZC
    int debug;                  /* initialized to FALSE */
    #endif

    NuError exit_stat;

    int maxbits;                /* initialized to DFLTBITS */
    int block_compress;         /* initialized to BLOCK_MASK */

    /*
     * Static local variables.  Some of these were explicitly initialized
     * to zero.
     */
    INTCODE oldmaxcode;         /* alloc_tables */
    HASH oldhashsize;           /* alloc_tables */
    int oldbits;                /* putcode */
    UCHAR outbuf[MAXBITS];      /* putcode */
    int prevbits;               /* nextcode */
    int size;                   /* nextcode */
    UCHAR inbuf[MAXBITS];       /* nextcode */
} LZCState;


/*
 * The following two parameter tables are the hash table sizes and
 * maximum code values for various code bit-lengths.  The requirements
 * are that Hashsize[n] must be a prime number and Maxcode[n] must be less
 * than Maxhash[n].  Table occupancy factor is (Maxcode - 256)/Maxhash.
 * Note:  I am using a lower Maxcode for 16-bit codes in order to
 * keep the hash table size less than 64k entries.
 */
static CONST HASH gNu_hs[] = {
    0x13FF,       /* 12-bit codes, 75% occupancy */
    0x26C3,       /* 13-bit codes, 80% occupancy */
    0x4A1D,       /* 14-bit codes, 85% occupancy */
    0x8D0D,       /* 15-bit codes, 90% occupancy */
    0xFFD9        /* 16-bit codes, 94% occupancy, 6% of code values unused */
};
#define Hashsize(maxb) (gNu_hs[(maxb) -MINBITS])

static CONST INTCODE gNu_mc[] = {
    0x0FFF,       /* 12-bit codes */
    0x1FFF,       /* 13-bit codes */
    0x3FFF,       /* 14-bit codes */
    0x7FFF,       /* 15-bit codes */
    0xEFFF        /* 16-bit codes, 6% of code values unused */
};
#define Maxcode(maxb) (gNu_mc[(maxb) -MINBITS])

#ifdef __STDC__
#ifdef DEBUG_LZC
#define allocx(type, ptr, size) \
    (((ptr) = (type FAR *) Nu_LZC_emalloc(pArchive, (unsigned int)(size),sizeof(type))) == NULLPTR(type) \
    ?   (DBUG(("%s: "#ptr" -- ", "LZC")), NOMEM) : OK \
    )
#else
#define allocx(type,ptr,size) \
    (((ptr) = (type FAR *) Nu_LZC_emalloc(pArchive, (unsigned int)(size),sizeof(type))) == NULLPTR(type) \
    ? NOMEM : OK \
    )
#endif
#else
#define allocx(type,ptr,size) \
    (((ptr) = (type FAR *) Nu_LZC_emalloc(pArchive, (unsigned int)(size),sizeof(type))) == NULLPTR(type) \
    ? NOMEM : OK \
    )
#endif

#define free_array(type,ptr,offset) \
    if (ptr != NULLPTR(type)) { \
        Nu_LZC_efree(pArchive, (ALLOCTYPE FAR *)((ptr) + (offset))); \
        (ptr) = NULLPTR(type); \
    }

  /*
   * Macro to allocate new memory to a pointer with an offset value.
   */
#define alloc_array(type, ptr, size, offset) \
    ( allocx(type, ptr, (size) - (offset)) != OK \
      ? NOMEM \
      : (((ptr) -= (offset)), OK) \
    )

/*static char FAR *sfx = NULLPTR(char) ;*/
#define suffix(code)     pLzcState->sfx[code]


#if (SPLIT_PFX)
    /*static CODE FAR *pfx[2] = {NULLPTR(CODE), NULLPTR(CODE)};*/
#else
    /*static CODE FAR *pfx = NULLPTR(CODE);*/
#endif


#if (SPLIT_HT)
    /*static CODE FAR *ht[2] = {NULLPTR(CODE),NULLPTR(CODE)};*/
#else
    /*static CODE FAR *ht = NULLPTR(CODE);*/
#endif


static int
Nu_LZC_alloc_tables(LZCState* pLzcState, INTCODE newmaxcode, HASH newhashsize)
{
    NuArchive* pArchive = pLzcState->pArchive;
    /*static INTCODE oldmaxcode = 0;*/
    /*static HASH oldhashsize = 0;*/

    if (newhashsize > pLzcState->oldhashsize) {
#if (SPLIT_HT)
        free_array(CODE,pLzcState->ht[1], 0);
        free_array(CODE,pLzcState->ht[0], 0);
#else
        free_array(CODE,pLzcState->ht, 0);
#endif
        pLzcState->oldhashsize = 0;
    }

    if (newmaxcode > pLzcState->oldmaxcode) {
#if (SPLIT_PFX)
        free_array(CODE,pLzcState->pfx[1], 128);
        free_array(CODE,pLzcState->pfx[0], 128);
#else
        free_array(CODE,pLzcState->pfx, 256);
#endif
        free_array(char,pLzcState->sfx, 256);

        if (   alloc_array(char, pLzcState->sfx, newmaxcode + 1, 256)
#if (SPLIT_PFX)
            || alloc_array(CODE, pLzcState->pfx[0], (newmaxcode + 1) / 2, 128)
            || alloc_array(CODE, pLzcState->pfx[1], (newmaxcode + 1) / 2, 128)
#else
            || alloc_array(CODE, pLzcState->pfx, (newmaxcode + 1), 256)
#endif
        ) {
            pLzcState->oldmaxcode = 0;
            pLzcState->exit_stat = NOMEM;
            return(NOMEM);
        }
        pLzcState->oldmaxcode = newmaxcode;
    }
    if (newhashsize > pLzcState->oldhashsize) {
        if (
#if (SPLIT_HT)
            alloc_array(CODE, pLzcState->ht[0], (newhashsize / 2) + 1, 0)
            || alloc_array(CODE, pLzcState->ht[1], newhashsize / 2, 0)
#else
            alloc_array(CODE, pLzcState->ht, newhashsize, 0)
#endif
        ) {
            pLzcState->oldhashsize = 0;
            pLzcState->exit_stat = NOMEM;
            return(NOMEM);
        }
        pLzcState->oldhashsize = newhashsize;
    }
    return (OK);
}

# if (SPLIT_PFX)
    /*
     * We have to split pfx[] table in half,
     * because it's potentially larger than 64k bytes.
     */
#   define prefix(code)   (pLzcState->pfx[(code) & 1][(code) >> 1])
# else
    /*
     * Then pfx[] can't be larger than 64k bytes,
     * or we don't care if it is, so we don't split.
     */
#   define prefix(code) (pLzcState->pfx[code])
# endif


/* The initializing of the tables can be done quicker with memset() */
/* but this way is portable through out the memory models.          */
/* If you use Microsoft halloc() to allocate the arrays, then       */
/* include the pragma #pragma function(memset)  and make sure that  */
/* the length of the memory block is not greater than 64K.          */
/* This also means that you MUST compile in a model that makes the  */
/* default pointers to be far pointers (compact or large models).   */
/* See the file COMPUSI.DOS to modify function emalloc().           */

# if (SPLIT_HT)
    /*
     * We have to split ht[] hash table in half,
     * because it's potentially larger than 64k bytes.
     */
#   define probe(hash)    (pLzcState->ht[(hash) & 1][(hash) >> 1])
#   define init_tables() \
    { \
      hash = pLzcState->hashsize >> 1; \
      pLzcState->ht[0][hash] = 0; \
      while (hash--) pLzcState->ht[0][hash] = pLzcState->ht[1][hash] = 0; \
      pLzcState->highcode = ~(~(INTCODE)0 << (pLzcState->bits = INITBITS)); \
      pLzcState->nextfree = (pLzcState->block_compress ? FIRSTFREE : 256); \
    }

# else

    /*
     * Then ht[] can't be larger than 64k bytes,
     * or we don't care if it is, so we don't split.
     */
#   define probe(hash) (pLzcState->ht[hash])
#   define init_tables() \
    { \
      hash = pLzcState->hashsize; \
      while (hash--) pLzcState->ht[hash] = 0; \
      pLzcState->highcode = ~(~(INTCODE)0 << (pLzcState->bits = INITBITS)); \
      pLzcState->nextfree = (pLzcState->block_compress ? FIRSTFREE : 256); \
    }

# endif


/*
 * ===========================================================================
 *      Compression
 * ===========================================================================
 */

static void
Nu_prratio(long int num, long int den)
{
    register int q;         /* Doesn't need to be long */

    if(num > 214748L) {     /* 2147483647/10000 */
        q = (int) (num / (den / 10000L));
    }
    else {
        q = (int) (10000L * num / den);     /* Long calculations, though */
    }
    if (q < 0) {
        DBUG(("-"));
        q = -q;
    }
    DBUG(("%d.%02d%%", q / 100, q % 100));
}

#ifdef COMP40
/* table clear for block compress */
/* this is for adaptive reset present in version 4.0 joe release */
/* DjG, sets it up and returns TRUE to compress and FALSE to not compress */
static int
Nu_LZC_cl_block(LZCState* pLzcState)     
{
    register long int rat;

    pLzcState->checkpoint = pLzcState->in_count + CHECK_GAP;
#ifdef DEBUG_LZC
    if ( pLzcState->debug ) {
        DBUG(( "count: %ld, ratio: ", pLzcState->in_count ));
        Nu_prratio ( pLzcState->in_count, pLzcState->bytes_out );
        DBUG(( "\n"));
    }
#endif

    if(pLzcState->in_count > 0x007fffff) { /* shift will overflow */
        rat = pLzcState->bytes_out >> 8;
        if(rat == 0)       /* Don't divide by zero */
            rat = 0x7fffffff;
        else
            rat = pLzcState->in_count / rat;
    }
    else
        rat = (pLzcState->in_count << 8) / pLzcState->bytes_out;  /* 8 fractional bits */

    if ( rat > pLzcState->ratio ){
        pLzcState->ratio = rat;
        return FALSE;
    }
    else {
        pLzcState->ratio = 0;
#ifdef DEBUG_LZC
        if(pLzcState->debug) {
            DBUG(( "clear\n" ));
        }
#endif
        return TRUE;    /* clear the table */
    }
    return FALSE; /* don't clear the table */
}
#endif

static CONST UCHAR gNu_rmask[9] = {0x00, 0x01, 0x03, 0x07, 0x0f, 0x1f, 0x3f, 0x7f, 0xff};

static void
Nu_LZC_putcode(LZCState* pLzcState, INTCODE code, register int bits)
{
    /*static int oldbits = 0;*/
    /*static UCHAR outbuf[MAXBITS];*/
    register UCHAR *buf;
    register int shift;

    if (bits != pLzcState->oldbits) {
        if (bits == 0) {
            /* bits == 0 means EOF, write the rest of the buffer. */
            if (pLzcState->offset > 0) {
                fwrite(pLzcState->outbuf,1,(pLzcState->offset +7) >> 3, pLzcState->outfp);
                pLzcState->bytes_out += ((pLzcState->offset +7) >> 3);
            }
            pLzcState->offset = 0;
            pLzcState->oldbits = 0;
            fflush(pLzcState->outfp);
            return;
        }
        else {
            /* Change the code size.  We must write the whole buffer,
             * because the expand side won't discover the size change
             * until after it has read a buffer full.
             */
            if (pLzcState->offset > 0) {
                fwrite(pLzcState->outbuf, 1, pLzcState->oldbits, pLzcState->outfp);
                pLzcState->bytes_out += pLzcState->oldbits;
                pLzcState->offset = 0;
            }
            pLzcState->oldbits = bits;
    #ifdef DEBUG_LZC
            if ( pLzcState->debug ) {
                DBUG(( "\nChange to %d bits\n", bits ));
            }
    #endif /* DEBUG_LZC */
        }
    }
    /*  Get to the first byte. */
    buf = pLzcState->outbuf + ((shift = pLzcState->offset) >> 3);
    if ((shift &= 7) != 0) {
        *(buf) |= (*buf & gNu_rmask[shift]) | (UCHAR)(code << shift);
        *(++buf) = (UCHAR)(code >> (8 - shift));
        if (bits + shift > 16)
            *(++buf) = (UCHAR)(code >> (16 - shift));
    }
    else {
        /* Special case for fast execution */
        *(buf) = (UCHAR)code;
        *(++buf) = (UCHAR)(code >> 8);
    }
    if ((pLzcState->offset += bits) == (bits << 3)) {
        pLzcState->bytes_out += bits;
        fwrite(pLzcState->outbuf,1,bits,pLzcState->outfp);
        pLzcState->offset = 0;
    }
    return;
}


#define kNuLZCEOF   (-1)

/*
 * Get the next byte from the input straw.  Also updates the CRC
 * if "doCalcCRC" is set to true.
 *
 * Returns kNuLZCEOF as the value when we're out of data.
 */
static NuError
Nu_LZCGetcCRC(LZCState* pLzcState, int* pSym)
{
    NuError err;
    uchar c;

    if (!pLzcState->uncompRemaining) {
        *pSym = kNuLZCEOF;
        return kNuErrNone;
    }

    err = Nu_StrawRead(pLzcState->pArchive, pLzcState->pStraw, &c, 1);
    if (err == kNuErrNone) {
        if (pLzcState->doCalcCRC)
            pLzcState->crc = Nu_CalcCRC16(pLzcState->crc, &c, 1);
        *pSym = c;
        pLzcState->uncompRemaining--;
    }

    return err;
}

/*
 * compress stdin to stdout
 */
static void
Nu_LZC_compress(LZCState* pLzcState, ulong* pDstLen)
{
    int c,adjbits;
    register HASH hash;
    register INTCODE code;
    HASH hashf[256];

    Assert(pLzcState->outfp != nil);

    pLzcState->maxcode = Maxcode(pLzcState->maxbits);
    pLzcState->hashsize = Hashsize(pLzcState->maxbits);

#ifdef COMP40
/* Only needed for adaptive reset */
    pLzcState->checkpoint = CHECK_GAP;
    pLzcState->ratio = 0;
#endif

    adjbits = pLzcState->maxbits -10;
    for (c = 256; --c >= 0; ){
        hashf[c] = ((( c &0x7) << 7) ^ c) << adjbits;
    }
    pLzcState->exit_stat = OK;
    if (Nu_LZC_alloc_tables(pLzcState, pLzcState->maxcode, pLzcState->hashsize))  /* exit_stat already set */
        return;
    init_tables();

    #if 0
    /* if not zcat or filter */
    if(is_list && !zcat_flg) {  /* Open output file */
        if (freopen(ofname, WRITE_FILE_TYPE, pLzcState->outfp) == NULL) {
            pLzcState->exit_stat = NOTOPENED;
            return;
        }
        if (!quiet)
            fprintf(stderr, "%s: ",ifname);     /*#if 0*/
        setvbuf(Xstdout,zbuf,_IOFBF,ZBUFSIZE);
    }
    #endif

    /*
    * Check the input stream for previously seen strings.  We keep
    * adding characters to the previously seen prefix string until we
    * get a character which forms a new (unseen) string.  We then send
    * the code for the previously seen prefix string, and add the new
    * string to our tables.  The check for previous strings is done by
    * hashing.  If the code for the hash value is unused, then we have
    * a new string.  If the code is used, we check to see if the prefix
    * and suffix values match the current input; if so, we have found
    * a previously seen string.  Otherwise, we have a hash collision,
    * and we try secondary hash probes until we either find the current
    * string, or we find an unused entry (which indicates a new string).
    */
    if (1 /*!nomagic*/) {
        putc(gNu_magic_header[0], pLzcState->outfp);
        putc(gNu_magic_header[1], pLzcState->outfp);
        putc((char)(pLzcState->maxbits | pLzcState->block_compress), pLzcState->outfp);
        if(ferror(pLzcState->outfp)){  /* check it on entry */
            pLzcState->exit_stat = WRITEERR;
            return;
        }
        pLzcState->bytes_out = 3L;     /* includes 3-byte header mojo */
    }
    else
        pLzcState->bytes_out = 0L;      /* no 3-byte header mojo */
    pLzcState->in_count = 1L;
    pLzcState->offset = 0;

    pLzcState->exit_stat = Nu_LZCGetcCRC(pLzcState, &c);
    if (pLzcState->exit_stat != kNuErrNone)
        return;
    pLzcState->prefxcode = (INTCODE)c;

    while (1) {
        pLzcState->exit_stat = Nu_LZCGetcCRC(pLzcState, &c);
        if (pLzcState->exit_stat != kNuErrNone)
            return;
        if (c == kNuLZCEOF)
            break;

        pLzcState->in_count++;
        hash = pLzcState->prefxcode ^ hashf[c];
        /* I need to check that my hash value is within range
        * because my 16-bit hash table is smaller than 64k.
        */
        if (hash >= pLzcState->hashsize)
            hash -= pLzcState->hashsize;
        if ((code = (INTCODE)probe(hash)) != UNUSED) {
            if (suffix(code) != (char)c || (INTCODE)prefix(code) != pLzcState->prefxcode) {
            /* hashdelta is subtracted from hash on each iteration of
            * the following hash table search loop.  I compute it once
            * here to remove it from the loop.
            */
                HASH hashdelta = (0x120 - c) << (adjbits);
                do  {
                    /* rehash and keep looking */
                    Assert(code >= FIRSTFREE && code <= pLzcState->maxcode);
                    if (hash >= hashdelta) hash -= hashdelta;
                        else hash += (pLzcState->hashsize - hashdelta);
                    Assert(hash < pLzcState->hashsize);
                    if ((code = (INTCODE)probe(hash)) == UNUSED)
                        goto newcode;
                } while (suffix(code) != (char)c || (INTCODE)prefix(code) != pLzcState->prefxcode);
            }
            pLzcState->prefxcode = code;
        }
        else {
            newcode: {
                Nu_LZC_putcode(pLzcState, pLzcState->prefxcode, pLzcState->bits);
                code = pLzcState->nextfree;
                Assert(hash < pLzcState->hashsize);
                Assert(code >= FIRSTFREE);
                Assert(code <= pLzcState->maxcode + 1);
                if (code <= pLzcState->maxcode) {
                    probe(hash) = (CODE)code;
                    prefix(code) = (CODE)pLzcState->prefxcode;
                    suffix(code) = (char)c;
                    if (code > pLzcState->highcode) {
                        pLzcState->highcode += code;
                        ++pLzcState->bits;
                    }
                    pLzcState->nextfree = code + 1;
                }
#ifdef COMP40
                else if (pLzcState->in_count >= pLzcState->checkpoint && pLzcState->block_compress ) {
                    if (Nu_LZC_cl_block(pLzcState)){
#else
                else if (pLzcState->block_compress){
#endif
                        Nu_LZC_putcode(pLzcState, (INTCODE)c, pLzcState->bits);
                        Nu_LZC_putcode(pLzcState, CLEAR, pLzcState->bits);
                        init_tables();
                        pLzcState->exit_stat = Nu_LZCGetcCRC(pLzcState, &c);
                        if (pLzcState->exit_stat != kNuErrNone)
                            return;
                        if (c == kNuLZCEOF)
                            break;
                        pLzcState->in_count++;
#ifdef COMP40
                    }
#endif
                }
                pLzcState->prefxcode = (INTCODE)c;
            }
        }
    }
    Nu_LZC_putcode(pLzcState, pLzcState->prefxcode, pLzcState->bits);
    Nu_LZC_putcode(pLzcState, CLEAR, 0);
    /*
     * Print out stats on stderr
     */
    if(1 /*zcat_flg == 0 && !quiet*/) {
#ifdef DEBUG_LZC
        DBUG(( 
            "%ld chars in, (%ld bytes) out, compression factor: ",
            pLzcState->in_count, pLzcState->bytes_out ));
        Nu_prratio( pLzcState->in_count, pLzcState->bytes_out );
        DBUG(( "\n"));
        DBUG(( "\tCompression as in compact: " ));
        Nu_prratio( pLzcState->in_count-pLzcState->bytes_out, pLzcState->in_count );
        DBUG(( "\n"));
        DBUG(( "\tLargest code (of last block) was %d (%d bits)\n",
            pLzcState->prefxcode - 1, pLzcState->bits ));
#else
        DBUG(( "Compression: " ));
        Nu_prratio( pLzcState->in_count-pLzcState->bytes_out, pLzcState->in_count );
#endif /* DEBUG_LZC */
    }
    if(pLzcState->bytes_out > pLzcState->in_count)      /*  if no savings */
        pLzcState->exit_stat = NOSAVING;
    *pDstLen = pLzcState->bytes_out;
    return ;
}


/*
 * NufxLib interface to LZC compression.
 */
static NuError
Nu_CompressLZC(NuArchive* pArchive, NuStraw* pStraw, FILE* fp,
    ulong srcLen, ulong* pDstLen, ushort* pCrc, int maxbits)
{
    NuError err = kNuErrNone;
    LZCState lzcState;

    memset(&lzcState, 0, sizeof(lzcState));
    lzcState.pArchive = pArchive;
    lzcState.pStraw = pStraw;
    lzcState.outfp = fp;
    lzcState.uncompRemaining = srcLen;

    if (pCrc == nil) {
        lzcState.doCalcCRC = false;
    } else {
        lzcState.doCalcCRC = true;
        lzcState.crc = *pCrc;
    }

    lzcState.maxbits = maxbits;
    lzcState.block_compress = BLOCK_MASK;     /* enabled */

    Nu_LZC_compress(&lzcState, pDstLen);
    err = lzcState.exit_stat;
    DBUG(("+++ LZC_compress returned with %d\n", err));

#if (SPLIT_HT)
    free_array(CODE,lzcState.ht[1], 0);
    free_array(CODE,lzcState.ht[0], 0);
#else
    free_array(CODE,lzcState.ht, 0);
#endif

#if (SPLIT_PFX)
    free_array(CODE,lzcState.pfx[1], 128);
    free_array(CODE,lzcState.pfx[0], 128);
#else
    free_array(CODE,lzcState.pfx, 256);
#endif
    free_array(char,lzcState.sfx, 256);

    if (pCrc != nil)
        *pCrc = lzcState.crc;

    return err;
}

NuError
Nu_CompressLZC12(NuArchive* pArchive, NuStraw* pStraw, FILE* fp,
    ulong srcLen, ulong* pDstLen, ushort* pCrc)
{
    return Nu_CompressLZC(pArchive, pStraw, fp, srcLen, pDstLen, pCrc, 12);
}

NuError
Nu_CompressLZC16(NuArchive* pArchive, NuStraw* pStraw, FILE* fp,
    ulong srcLen, ulong* pDstLen, ushort* pCrc)
{
    return Nu_CompressLZC(pArchive, pStraw, fp, srcLen, pDstLen, pCrc, 16);
}


/*
 * ===========================================================================
 *      Expansion
 * ===========================================================================
 */

/*
 * Write the next byte to the output funnel.  Also updates the CRC
 * if "doCalcCRC" is set to true.
 *
 * Returns kNuLZCEOF as the value when we're out of data.
 */
static NuError
Nu_LZCPutcCRC(LZCState* pLzcState, char c)
{
    NuError err;

    err = Nu_FunnelWrite(pLzcState->pArchive, pLzcState->pFunnel,
            (uchar*) &c, 1);
    if (pLzcState->doCalcCRC)
        pLzcState->crc = Nu_CalcCRC16(pLzcState->crc, (uchar*) &c, 1);

    return err;
}


static int
Nu_LZC_nextcode(LZCState* pLzcState, INTCODE* codeptr)
/* Get the next code from input and put it in *codeptr.
 * Return (TRUE) on success, or return (FALSE) on end-of-file.
 * Adapted from COMPRESS V4.0.
 */
{
    /*static int prevbits = 0;*/
    register INTCODE code;
    /*static int size;*/
    /*static UCHAR inbuf[MAXBITS];*/
    register int shift;
    UCHAR *bp;

    /* If the next entry is a different bit-size than the preceeding one
    * then we must adjust the size and scrap the old buffer.
    */
    if (pLzcState->prevbits != pLzcState->bits) {
        pLzcState->prevbits = pLzcState->bits;
        pLzcState->size = 0;
    }
    /* If we can't read another code from the buffer, then refill it.
    */
    shift = pLzcState->offset;
    if (pLzcState->size - shift < pLzcState->bits) {
        /* Read more input and convert size from # of bytes to # of bits */
        long getSize;

        getSize = pLzcState->bits;
        if (getSize > pLzcState->compRemaining)
            getSize = pLzcState->compRemaining;
        if (!getSize)       /* act like EOF */
            return FALSE;
        pLzcState->size = fread(pLzcState->inbuf, 1, getSize, pLzcState->infp) << 3;
        if (pLzcState->size <= 0 || ferror(pLzcState->infp))
            return(FALSE);
        pLzcState->compRemaining -= getSize;
        pLzcState->offset = shift = 0;
    }
    /* Get to the first byte. */
    bp = pLzcState->inbuf + (shift >> 3);
    /* Get first part (low order bits) */
    code = (*bp++ >> (shift &= 7));
    /* high order bits. */
    code |= *bp++ << (shift = 8 - shift);
    if ((shift += 8) < pLzcState->bits) code |= *bp << shift;
        *codeptr = code & pLzcState->highcode;
    pLzcState->offset += pLzcState->bits;
    return (TRUE);
}

static void
Nu_LZC_decompress(LZCState* pLzcState, ulong compressedLen)
{
    NuArchive* pArchive = pLzcState->pArchive;
    register int i;
    register INTCODE code;
    char sufxchar = 0;
    INTCODE savecode;
    FLAG fulltable = FALSE, cleartable;
    /*static*/ char *token= NULL;         /* String buffer to build token */
    /*static*/ int maxtoklen = MAXTOKLEN;
    int flags;

    Assert(pLzcState->infp != nil);

    pLzcState->exit_stat = OK;

    if (compressedLen < 3) {
        /* not long enough to be valid! */
        pLzcState->exit_stat = kNuErrBadData;
        Nu_ReportError(NU_BLOB, pLzcState->exit_stat, "thread too short to be valid LZC");
        return;
    }
    pLzcState->compRemaining = compressedLen;

    /*
     * This comes out of "compress.c" rather than "compapi.c".
     */
    if ((getc(pLzcState->infp)!=(gNu_magic_header[0] & 0xFF))
        || (getc(pLzcState->infp)!=(gNu_magic_header[1] & 0xFF)))
    {
        DBUG(("not in compressed format\n"));
        pLzcState->exit_stat = kNuErrBadData;
        return;
    }
    flags = getc(pLzcState->infp);    /* set -b from file */
    pLzcState->block_compress = flags & BLOCK_MASK;
    pLzcState->maxbits = flags & BIT_MASK;
    if(pLzcState->maxbits > MAXBITS) {
        DBUG(("compressed with %d bits, can only handle %d bits\n",
            pLzcState->maxbits, MAXBITS));
        pLzcState->exit_stat = kNuErrBadData;
        return;
    }

    pLzcState->compRemaining -= 3;

    /* Initialze the token buffer. */
    token = (char*)Nu_Malloc(pArchive, maxtoklen);
    if (token == NULL) {
        pLzcState->exit_stat = NOMEM;
        return;
    }

    if (Nu_LZC_alloc_tables(pLzcState, pLzcState->maxcode = ~(~(INTCODE)0 << pLzcState->maxbits),0)) /* exit_stat already set */
        return;

    #if 0
    /* if not zcat or filter */
    if(is_list && !zcat_flg) {  /* Open output file */
        if (freopen(ofname, WRITE_FILE_TYPE, stdout) == NULL) {
            pLzcState->exit_stat = NOTOPENED;
            return;
        }
        if (!quiet)
            fprintf(stderr, "%s: ",ifname);     /*#if 0*/
        setvbuf(stdout,xbuf,_IOFBF,XBUFSIZE);
    }
    #endif

    cleartable = TRUE;
    savecode = CLEAR;
    pLzcState->offset = 0;
    do {
        if ((code = savecode) == CLEAR && cleartable) {
            pLzcState->highcode = ~(~(INTCODE)0 << (pLzcState->bits = INITBITS));
            fulltable = FALSE;
            pLzcState->nextfree = (cleartable = pLzcState->block_compress) == FALSE ? 256 : FIRSTFREE;
            if (!Nu_LZC_nextcode(pLzcState, &pLzcState->prefxcode))
                break;
            /*putc((*/sufxchar = (char)pLzcState->prefxcode/*), stdout)*/;
            pLzcState->exit_stat = Nu_LZCPutcCRC(pLzcState, sufxchar);
            if (pLzcState->exit_stat != kNuErrNone)
                return;
            continue;
        }
        i = 0;
        if (code >= pLzcState->nextfree && !fulltable) {
            if (code != pLzcState->nextfree){
                DBUG(("ERROR: code (0x%x) != nextfree (0x%x)\n",
                    code, pLzcState->nextfree));
                pLzcState->exit_stat = CODEBAD;
                return ;     /* Non-existant code */
            }
            /* Special case for sequence KwKwK (see text of article)         */
            code = pLzcState->prefxcode;
            token[i++] = sufxchar;
        }
        /* Build the token string in reverse order by chasing down through
         * successive prefix tokens of the current token.  Then output it.
         */
        while (code >= 256) {
            #ifdef DEBUG_LZC
            /* These are checks to ease paranoia. Prefix codes must decrease
             * monotonically, otherwise we must have corrupt tables.  We can
             * also check that we haven't overrun the token buffer.
             */
            if (code <= (INTCODE)prefix(code)){
                pLzcState->exit_stat= TABLEBAD;
                return;
            }
            #endif
            if (i >= maxtoklen) {
                maxtoklen *= 2;   /* double the size of the token buffer */
                if ((token = Nu_Realloc(pArchive, token, maxtoklen)) == NULL) {
                    pLzcState->exit_stat = TOKTOOBIG;
                    return;
                }
            }
            token[i++] = suffix(code);
            code = (INTCODE)prefix(code);
        }
        /*putc(*/sufxchar = (char)code/*, stdout)*/;
        pLzcState->exit_stat = Nu_LZCPutcCRC(pLzcState, sufxchar);
        while (--i >= 0) {
            /*putc(token[i], stdout);*/
            pLzcState->exit_stat = Nu_LZCPutcCRC(pLzcState, token[i]);
        }
        if (pLzcState->exit_stat != kNuErrNone)
            return;
        /* If table isn't full, add new token code to the table with
         * codeprefix and codesuffix, and remember current code.
         */
        if (!fulltable) {
            code = pLzcState->nextfree;
            Assert(256 <= code && code <= pLzcState->maxcode);
            prefix(code) = (CODE)pLzcState->prefxcode;
            suffix(code) = sufxchar;
            pLzcState->prefxcode = savecode;
            if (code++ == pLzcState->highcode) {
                if (pLzcState->highcode >= pLzcState->maxcode) {
                    fulltable = TRUE;
                    --code;
                }
                else {
                    ++pLzcState->bits;
                    pLzcState->highcode += code;  /* nextfree == highcode + 1 */
                }
            }
            pLzcState->nextfree = code;
        }
    } while (Nu_LZC_nextcode(pLzcState, &savecode));
    pLzcState->exit_stat = (ferror(pLzcState->infp))? READERR : OK;

    Nu_Free(pArchive, token);
    return ;
}


/*
 * NufxLib interface to LZC expansion.
 */
NuError
Nu_ExpandLZC(NuArchive* pArchive, const NuRecord* pRecord,
    const NuThread* pThread, FILE* infp, NuFunnel* pFunnel, ushort* pCrc)
{
    NuError err = kNuErrNone;
    LZCState lzcState;

    memset(&lzcState, 0, sizeof(lzcState));
    lzcState.pArchive = pArchive;
    lzcState.infp = infp;
    lzcState.pFunnel = pFunnel;

    if (pCrc == nil) {
        lzcState.doCalcCRC = false;
    } else {
        lzcState.doCalcCRC = true;
        lzcState.crc = *pCrc;
    }

    Nu_LZC_decompress(&lzcState, pThread->thCompThreadEOF);
    err = lzcState.exit_stat;
    DBUG(("+++ LZC_decompress returned with %d\n", err));

#if (SPLIT_HT)
    free_array(CODE,lzcState.ht[1], 0);
    free_array(CODE,lzcState.ht[0], 0);
#else
    free_array(CODE,lzcState.ht, 0);
#endif

#if (SPLIT_PFX)
    free_array(CODE,lzcState.pfx[1], 128);
    free_array(CODE,lzcState.pfx[0], 128);
#else
    free_array(CODE,lzcState.pfx, 256);
#endif
    free_array(char,lzcState.sfx, 256);

    if (pCrc != nil)
        *pCrc = lzcState.crc;
    return err;
}

#endif /*ENABLE_LZC*/
