/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Pack and unpack DDD format.
 */
/*
The trouble with unpacking DOS DDD 2.x files:

[ Most of this is no longer relevant, but the discussion is enlightening. ]

DDD writes its files as DOS 3.3 binary (type 'B') files, which have
starting address and length embedded as the first 4 bytes.  Unfortunately, it
cannot write the length, because the largest possible 16-bit length value is
only 64K.  Instead, DDD sets it to zero.  DDD v2.0 does store a copy of the
length *in sectors* in the filename (e.g. "<397>"), but this doesn't really
help much.  When CiderPress goes to extract or view the file, it just sees a
zero-length binary file.

CiderPress could make an exception and assume that any binary file with
zero length and more than one sector allocated has a length equal to the
number of sectors times 256.  This could cause problems for other things,
but it's probably pretty safe.  However, we still don't have an accurate
idea of where the end of the file is.

Knowing where the file ends is important because there is no identifying
information or checksum in a DDD file.  The only way to know that it's a
DDD compressed disk is to try to unpack it and see if you end up at exactly
140K at the same time that you run out of input.  Without knowing where the
file really ends this test is much less certain.  (DDD 2.5 appears to have
added some sort of checksum, which was appended to the DOS filename, but
without knowing how it was calculated there's no way to verify it.)

The only safe way to make this work would be to skip the automatic format
detection and tell CiderPress that the file is definitely DDD format.
There's currently no easy way to do that without complicating the user
interface.  Filename extensions might be useful in making the decision,
but they're rare under DOS 3.3, and I don't know if the "<397>" convention
is common to all versions of DDD.

Complicating the matter is that, if a DOS DDD file (type 'B') is converted
to ProDOS, the first 4 bytes will be stripped off.  Without unpacking
the file and knowing to within a byte where it ends, there's no way to
automatically tell whether to start at byte 0 or byte 4.  (DDD Pro files
have four bytes of garbage at the very start, probably in an attempt to
retain compatibility with the DOS version.  Because it uses REL files the
4 bytes of extra DOS stuff aren't added when the files are copied around,
so this was a reasonably smart thing to do, but it complicates matters
for CiderPress because a file extracted from DOS and a file extracted
from ProDOS will come out differently because only the DOS version has the
leading 4 bytes stripped.  This could be avoided if the DOS file uses the
'R' or 'S' file type, but we still lack an accurate file length.)

To unpack a file created by DOS DDD v2.x with CiderPress:
 - In an emulator, copy the file to a ProDOS disk, using something that
   guesses at the actual length when one isn't provided (Copy ][+ 9.0
   may work).
 - Reduce the length to within a byte or two of the actual end of file.
   This is nearly impossible, because DDD doesn't zero out the remaining
   data in the last sector.
 - Insert 4 bytes of garbage at the front of the file.  My copy of DDD
   Pro 1.1 seems to like 03 c9 bf d0.
Probably not worth the effort.  Just unpack it with an emulator.

In general DDD is a rather poor choice, because the compression isn't
very good and there's no checksum.  ShrinkIt is a much better way to go.

NOTE: DOS DDD v2.0 seems to have a bug where it doesn't always write the last
run correctly.  On the DOS system master this caused the last half of the
last sector (FID's T/S list) to have garbage written instead of zero bytes,
which caused CP to label FID as damaged.  DDD v2.1 and later doesn't
appear to have this issue.  Unfortunate that DDD 2.0 is what shipped on the
SST disk.
 */
#include "StdAfx.h"
#include "DiskImgPriv.h"

const int kNumSymbols = 256;
const int kNumFavorites = 20;
const int kRLEDelim = 0x97;     // value MUST have high bit set
const int kMaxExcessByteCount = WrapperDDD::kMaxDDDZeroCount + 1;
//const int kTrackLen = 4096;
//const int kNumTracks = 35;

/* I suspect this is random garbage, but it's appearing consistently */
const unsigned long kDDDProSignature = 0xd0bfc903;


/*
 * ===========================================================================
 *      BitBuffer
 * ===========================================================================
 */

/*
 * Class for getting and putting bits to and from a file.
 */
class WrapperDDD::BitBuffer {
public:
    BitBuffer(void) : fpGFD(NULL), fBits(0), fBitCount(0), fIOFailure(false) {}
    ~BitBuffer(void) {}

    void SetFile(GenericFD* pGFD) { fpGFD = pGFD; }
    void PutBits(uint8_t bits, int numBits);
    uint8_t GetBits(int numBits);

    bool IOFailure(void) const { return fIOFailure; }

    static uint8_t Reverse(uint8_t val);

private:
    GenericFD*  fpGFD;
    uint8_t     fBits;
    int         fBitCount;
    bool        fIOFailure;
};

/*
 * Add bits to the buffer.
 *
 * We roll the low bits out of "bits" and shift them to the left (in the
 * reverse order in which they were passed in).  As soon as we get 8 bits
 * we flush.
 */
void WrapperDDD::BitBuffer::PutBits(uint8_t bits, int numBits)
{
    assert(fBitCount >= 0 && fBitCount < 8);
    assert(numBits > 0 && numBits <= 8);
    assert(fpGFD != NULL);

    DIError dierr;

    while (numBits--) {
        fBits = (fBits << 1) | (bits & 0x01);
        fBitCount++;

        if (fBitCount == 8) {
            dierr = fpGFD->Write(&fBits, 1);
            fIOFailure = (dierr != kDIErrNone);
            fBitCount = 0;
        }

        bits >>= 1;
    }
}

/*
 * Get bits from the buffer.
 *
 * These come out in the order in which they appear in the file, which
 * means that in some cases they will have to be reversed.
 */
uint8_t WrapperDDD::BitBuffer::GetBits(int numBits)
{
    assert(fBitCount >= 0 && fBitCount < 8);
    assert(numBits > 0 && numBits <= 8);
    assert(fpGFD != NULL);

    DIError dierr;
    uint8_t retVal;

    if (fBitCount == 0) {
        /* have no bits */
        dierr = fpGFD->Read(&fBits, 1);
        fIOFailure = (dierr != kDIErrNone);
        fBitCount = 8;
    }

    if (numBits <= fBitCount) {
        /* just serve up what we've already got */
        retVal = fBits >> (8 - numBits);
        fBits <<= numBits;
        fBitCount -= numBits;
    } else {
        /* some old, some new; load what we have right-aligned */
        retVal = fBits >> (8 - fBitCount);
        numBits -= fBitCount;

        dierr = fpGFD->Read(&fBits, 1);
        fIOFailure = (dierr != kDIErrNone);
        fBitCount = 8;

        /* make room for the rest (also zeroes out the low bits) */
        retVal <<= numBits;

        /* add the high bits from the new byte */
        retVal |= fBits >> (8 - numBits);
        fBits <<= numBits;
        fBitCount -= numBits;
    }

    return retVal;
}

/*
 * Utility function to reverse the order of bits in a byte.
 */
/*static*/ uint8_t WrapperDDD::BitBuffer::Reverse(uint8_t val)
{
    int i;
    uint8_t result = 0;       // init is to make valgrind happy

    for (i = 0; i < 8; i++) {
        result = (result << 1) + (val & 0x01);
        val >>= 1;
    }

    return result;
}


/*
 * ===========================================================================
 *      DDD compression functions
 * ===========================================================================
 */

/*
 * These are all odd, which when they're written in reverse order means
 * they all have their hi bits set.
 */
static const uint8_t kFavoriteBitEnc[kNumFavorites] = {
    0x03, 0x09, 0x1f, 0x0f, 0x07, 0x1b, 0x0b, 0x0d, 0x15, 0x37,
    0x3d, 0x25, 0x05, 0xb1, 0x11, 0x21, 0x01, 0x57, 0x5d, 0x1d
};
static const int kFavoriteBitEncLen[kNumFavorites] = {
    4, 4, 5, 5, 5, 5, 5, 5, 5, 6,
    6, 6, 6, 6, 6, 6, 6, 7, 7, 7
};


/*
 * Pack a disk image with DDD.
 *
 * Assumes pSrcGFD points to DOS-ordered sectors.  (This is enforced when the
 * disk image is first being created.)
 */
/*static*/ DIError WrapperDDD::PackDisk(GenericFD* pSrcGFD, GenericFD* pWrapperGFD,
    short diskVolNum)
{
    DIError dierr = kDIErrNone;
    BitBuffer bitBuffer;

    assert(diskVolNum >= 0 && diskVolNum < 256);

    /* write four zeroes to replace the DOS addr/len bytes */
    /* (actually, let's write the apparent DDD Pro v1.1 signature instead) */
    WriteLongLE(pWrapperGFD, kDDDProSignature);

    bitBuffer.SetFile(pWrapperGFD);

    bitBuffer.PutBits(0x00, 3);
    bitBuffer.PutBits((uint8_t)diskVolNum, 8);

    /*
     * Process all tracks.
     */
    for (int track = 0; track < kNumTracks; track++) {
        uint8_t trackBuf[kTrackLen];

        dierr = pSrcGFD->Read(trackBuf, kTrackLen);
        if (dierr != kDIErrNone) {
            LOGI(" DDD error during read (err=%d)", dierr);
            goto bail;
        }

        PackTrack(trackBuf, &bitBuffer);
    }

    /* write 8 bits of zeroes to flush remaining data out of buffer */
    bitBuffer.PutBits(0x00, 8);

    /* write another zero byte because that's what DDD Pro v1.1 does */
    long zero;
    zero = 0;
    dierr = pWrapperGFD->Write(&zero, 1);
    if (dierr != kDIErrNone)
        goto bail;

    assert(dierr == kDIErrNone);
bail:
    return dierr;
}

/*
 * Compress a track full of data.
 */
/*static*/ void WrapperDDD::PackTrack(const uint8_t* trackBuf, BitBuffer* pBitBuf)
{
    uint16_t freqCounts[kNumSymbols];
    uint8_t favorites[kNumFavorites];
    int i, fav;

    ComputeFreqCounts(trackBuf, freqCounts);
    ComputeFavorites(freqCounts, favorites);

    /* write favorites */
    for (fav = 0; fav < kNumFavorites; fav++)
        pBitBuf->PutBits(favorites[fav], 8);

    /*
     * Compress track data.  Store runs as { 0x97 char count }, where
     * a count of zero means 256.
     */
    const uint8_t* ucp = trackBuf;
    for (i = 0; i < kTrackLen; i++, ucp++) {
        if (i < (kTrackLen-3) &&
            *ucp == *(ucp+1) &&
            *ucp == *(ucp+2) &&
            *ucp == *(ucp+3))
        {
            int runLen = 4;
            i += 3;
            ucp += 3;

            while (i < kTrackLen-1 && *ucp == *(ucp+1)) {
                runLen++;
                ucp++;
                i++;

                if (runLen == 256) {
                    runLen = 0;
                    break;
                }
            }

            pBitBuf->PutBits(kRLEDelim, 8);     // note kRLEDelim has hi bit set
            pBitBuf->PutBits(*ucp, 8);
            pBitBuf->PutBits(runLen, 8);

        } else {
            /*
             * Not a run, see if it's one of our favorites.
             */
            for (fav = 0; fav < kNumFavorites; fav++) {
                if (*ucp == favorites[fav])
                    break;
            }
            if (fav == kNumFavorites) {
                /* just a plain byte */
                pBitBuf->PutBits(0x00, 1);
                pBitBuf->PutBits(*ucp, 8);
            } else {
                /* found a favorite; leading hi bit is implied */
                pBitBuf->PutBits(kFavoriteBitEnc[fav], kFavoriteBitEncLen[fav]);
            }
        }
    }
}


/*
 * Compute the #of times each byte appears in trackBuf.  Runs of four
 * bytes or longer are completely ignored.
 *
 * "trackBuf" holds kTrackLen bytes of data, and "freqCounts" holds
 * kNumSymbols (256) 16-bit values.
 */
/*static*/ void WrapperDDD::ComputeFreqCounts(const uint8_t* trackBuf,
    uint16_t* freqCounts)
{
    const uint8_t* ucp;
    int i;

    memset(freqCounts, 0, 256 * sizeof(uint16_t));

    ucp = trackBuf;
    for (i = 0; i < kTrackLen; i++, ucp++) {
        if (i < (kTrackLen-3) &&
            *ucp == *(ucp+1) &&
            *ucp == *(ucp+2) &&
            *ucp == *(ucp+3))
        {
            int runLen = 4; // DEBUG only
            i += 3;
            ucp += 3;

            while (i < kTrackLen-1 && *ucp == *(ucp+1)) {
                runLen++;
                ucp++;
                i++;

                if (runLen == 256) {
                    runLen = 0;
                    break;
                }
            }

            //LOGI("Found run of %d of 0x%02x", runLen, *ucp);
        } else {
            /* not a run, just update stats */
            freqCounts[*ucp]++;
        }
    }
}

/*
 * Find the 20 most frequently occurring symbols, in order.
 *
 * Modifies "freqCounts".
 */
/*static*/ void WrapperDDD::ComputeFavorites(uint16_t* freqCounts,
    uint8_t* favorites)
{
    int i, fav;

    for (fav = 0; fav < kNumFavorites; fav++) {
        uint16_t bestCount = 0;
        uint8_t bestSym = 0;

        for (i = 0; i < kNumSymbols; i++) {
            if (freqCounts[i] >= bestCount) {
                bestSym = (uint8_t) i;
                bestCount = freqCounts[i];
            }
        }

        favorites[fav] = bestSym;
        freqCounts[bestSym] = 0;
    }

    //LOGI("FAVORITES: ");
    //for (fav = 0; fav < kNumFavorites; fav++)
    //    LOGI("%02x", favorites[fav]);
    //LOGI("");
}


/*
 * ===========================================================================
 *      DDD expansion functions
 * ===========================================================================
 */

/*
 * This is the reverse of the kFavoriteBitEnc table.  The bits are
 * reversed and lack the high bit.
 */
static const uint8_t kFavoriteBitDec[kNumFavorites] = {
    0x04, 0x01, 0x0f, 0x0e, 0x0c, 0x0b, 0x0a, 0x06, 0x05, 0x1b,
    0x0f, 0x09, 0x08, 0x03, 0x02, 0x01, 0x00, 0x35, 0x1d, 0x1c
};

/*
 * Entry point for unpacking a disk image compressed with DDD.
 *
 * The result is an unadorned DOS-ordered image.
 */
/*static*/ DIError WrapperDDD::UnpackDisk(GenericFD* pGFD, GenericFD* pNewGFD,
    short* pDiskVolNum)
{
    DIError dierr = kDIErrNone;
    BitBuffer bitBuffer;
    uint8_t val;
    long lbuf;

    assert(pGFD != NULL);
    assert(pNewGFD != NULL);

    /* read four zeroes to skip the DOS addr/len bytes */
    assert(sizeof(lbuf) >= 4);
    dierr = pGFD->Read(&lbuf, 4);
    if (dierr != kDIErrNone)
        goto bail;

    bitBuffer.SetFile(pGFD);

    val = bitBuffer.GetBits(3);
    if (val != 0) {
        LOGI(" DDD bits not zero, this isn't a DDD II file (0x%02x)", val);
        dierr = kDIErrGeneric;
        goto bail;
    }
    val = bitBuffer.GetBits(8);
    *pDiskVolNum = bitBuffer.Reverse(val);
    LOGI(" DDD found disk volume num = %d", *pDiskVolNum);

    int track;
    for (track = 0; track < kNumTracks; track++) {
        uint8_t trackBuf[kTrackLen];

        if (!UnpackTrack(&bitBuffer, trackBuf)) {
            LOGI(" DDD failed unpacking track %d", track);
            dierr = kDIErrBadCompressedData;
            goto bail;
        }
        if (bitBuffer.IOFailure()) {
            LOGI(" DDD failure or EOF on input file");
            dierr = kDIErrBadCompressedData;
            goto bail;
        }
        dierr = pNewGFD->Write(trackBuf, kTrackLen);
        if (dierr != kDIErrNone)
            goto bail;
    }

    /*
     * We should be within a byte or two of the end of the file.  Try
     * to read more and expect it to fail.
     *
     * Unfortunately, if this was a DOS DDD file, we could be up to 256
     * bytes off (the 1 additional byte it adds plus the remaining 255
     * bytes in the sector).  We have to choose between a tight auto-detect
     * and the ability to process DOS DDD files.
     *
     * Fortunately the need to hit track boundaries exactly and the quick test
     * for long runs of bytes provides some opportunity for correct
     * detection.
     */
    size_t actual;
    char sctBuf[256 + 16];
    dierr = pGFD->Read(&sctBuf, sizeof(sctBuf), &actual);
    if (dierr == kDIErrNone) {
        if (actual > /*kMaxExcessByteCount*/ 256) {
            LOGW(" DDD looks like too much data in input file (%lu extra)",
                (unsigned long) actual);
            dierr = kDIErrBadCompressedData;
            goto bail;
        } else {
            LOGI(" DDD excess bytes (%lu) within normal parameters",
                (unsigned long) actual);
        }
    }

    LOGI(" DDD looks like a DDD archive!");
    dierr = kDIErrNone;

bail:
    return dierr;
}

/*
 * Unpack a single track.
 *
 * Returns "true" if all went well, "false" if something failed.
 */
/*static*/ bool WrapperDDD::UnpackTrack(BitBuffer* pBitBuffer, uint8_t* trackBuf)
{
    uint8_t favorites[kNumFavorites];
    uint8_t val;
    uint8_t* trackPtr;
    int fav;

    /*
     * Start by pulling our favorites out, in reverse order.
     */
    for (fav = 0; fav < kNumFavorites; fav++) {
        val = pBitBuffer->GetBits(8);
        val = pBitBuffer->Reverse(val);
        favorites[fav] = val;
    }

    trackPtr = trackBuf;

    /*
     * Keep pulling data out until the track is full.
     */
    while (trackPtr < trackBuf + kTrackLen) {
        val = pBitBuffer->GetBits(1);
        if (!val) {
            /* simple byte */
            val = pBitBuffer->GetBits(8);
            val = pBitBuffer->Reverse(val);
            *trackPtr++ = val;
        } else {
            /* try for a prefix match */
            int extraBits;

            val = pBitBuffer->GetBits(2);

            for (extraBits = 0; extraBits < 4; extraBits++) {
                val = (val << 1) | pBitBuffer->GetBits(1);
                int start, end;

                if (extraBits == 0) {
                    start = 0;
                    end = 2;
                } else if (extraBits == 1) {
                    start = 2;
                    end = 9;
                } else if (extraBits == 2) {
                    start = 9;
                    end = 17;
                } else {
                    start = 17;
                    end = 20;
                }

                while (start < end) {
                    if (val == kFavoriteBitDec[start]) {
                        /* winner! */
                        *trackPtr++ = favorites[start];
                        break;
                    }
                    start++;
                }
                if (start != end)
                    break;      // we got it, break out of for loop
            }
            if (extraBits == 4) {
                /* we didn't get it, this must be RLE */
                uint8_t rleChar;
                int rleCount;

                (void) pBitBuffer->GetBits(1);  // get last bit of 0x97
                val = pBitBuffer->GetBits(8);
                rleChar = pBitBuffer->Reverse(val);
                val = pBitBuffer->GetBits(8);
                rleCount = pBitBuffer->Reverse(val);
                //LOGI(" DDD found run of %d of 0x%02x", rleCount, rleChar);

                if (rleCount == 0)
                    rleCount = 256;

                /* make sure we won't overrun */
                if (trackPtr + rleCount > trackBuf + kTrackLen) {
                    LOGI(" DDD overrun in RLE");
                    return false;
                }
                while (rleCount--)
                    *trackPtr++ = rleChar;
            }
        }
    }

    return true;
}
