/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Pack DDD.

The trouble with unpacking DOS DDD 2.x files:

The files are stored as binary files with no length.  DDD v2.0 stored
a copy of the length in sectors in the filename (e.g. "<397>").  This
means that, when CiderPress goes to extract or view the file, it just
sees an empty binary file.

CiderPress could make an exception and assume that any binary file with
zero length and more than one sector allocated has a length equal to the
number of sectors times 256.  This could cause problems for other things,
but it's probably pretty safe.  However, we still don't have an accurate
idea of where the end of the file is.

Knowing where the file ends is important because there is no identifying
information or checksum in a DDD file.  The only way to know that it's a
DDD compressed disk is to try to unpack it and see if you end up at exactly
140K at the same time that you run out of input.  Without knowing where the
file really ends, this test is much less certain.

The only safe way to make this work would be to skip the automatic format
detection and tell CiderPress that the file is definitely DDD format.
There's currently no easy way to do that without complicating the user
interface.  Filename extensions might be useful, but they're rare under
DOS 3.3, and I don't think the "<397>" convention is common to all versions
of DDD.

Complicating the matter is that, if a DOS DDD file (type 'B') is converted
to ProDOS, the first 4 bytes will be stripped off.  Without unpacking
the file and knowing to within a byte where it ends, there's no way to
automatically tell whether to start at byte 0 or byte 4.  (DDD Pro files
have four bytes of garbage at the very start, probably in an attempt to
retain compatibility with the DOS version.  Because it uses REL files the
4 bytes of extra DOS stuff aren't added when the files are copied around,
so this was a reasonably smart thing to do, but it complicates matters
for CiderPress because a file extracted from DOS and a file extracted
from ProDOS will come out differently due to the 4 bytes of type 'B'
gunk getting stripped.  This can be avoided if the DOS file uses the 'R'
or 'S' file type.)

All this would have been much easier if the DOS files had a length word.

To unpack a file created by DOS DDD v2.x:
 - Copy the file to a ProDOS disk, using something that guesses at the
   actual length when one isn't provided (Copy ][+ 9.0 may work).
 - Reduce the length to within a byte or two of the actual end of file.
   Removing all but the last couple of trailing zero bytes usually does
   the trick.
 - Insert 4 bytes of garbage at the front of the file.  My copy of DDD
   Pro 1.1 seems to like 03 c9 bf d0.
Probably not worth the effort.
 */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include "../diskimg/DiskImg.h"
#include "../nufxlib/NufxLib.h"

using namespace DiskImgLib;

#define nil NULL
#define NELEM(x) (sizeof(x) / sizeof((x)[0]))

FILE* gLog = nil;
pid_t gPid = getpid();

const int kTrackLen = 4096;
const int kNumSymbols = 256;
const int kNumFavorites = 20;
const int kRLEDelim = 0x97;     // value MUST have high bit set
const int kNumTracks = 35;

/* I suspect this is random garbage, but it's consistent for me */
const unsigned long kDDDProSignature = 0xd0bfc903;


/*
 * Class for getting and putting bits to and from a file.
 */
class BitBuffer {
public:
    BitBuffer(void) : fFp(nil), fBits(0), fBitCount(0) {}
    ~BitBuffer(void) {}

    void SetFile(FILE* fp) { fFp = fp; }
    void PutBits(unsigned char bits, int numBits);
    //void FlushBits(void);
    unsigned char GetBits(int numBits);

    static unsigned char Reverse(unsigned char val);

private:
    FILE*           fFp;
    unsigned char   fBits;
    int             fBitCount;
};

/*
 * Add bits to the buffer.
 *
 * We roll the low bits out of "bits" and shift them to the left (in the
 * reverse order in which they were passed in).  As soon as we get 8 bits
 * we flush.
 */
void
BitBuffer::PutBits(unsigned char bits, int numBits)
{
    assert(fBitCount >= 0 && fBitCount < 8);
    assert(numBits > 0 && numBits <= 8);
    assert(fFp != nil);

    while (numBits--) {
        fBits = (fBits << 1) | (bits & 0x01);
        fBitCount++;

        if (fBitCount == 8) {
            putc(fBits, fFp);
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
unsigned char
BitBuffer::GetBits(int numBits)
{
    assert(fBitCount >= 0 && fBitCount < 8);
    assert(numBits > 0 && numBits <= 8);
    assert(fFp != nil);

    unsigned char retVal;

    if (fBitCount == 0) {
        /* have no bits */
        fBits = getc(fFp);
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

        fBits = getc(fFp);
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
/*static*/ unsigned char
BitBuffer::Reverse(unsigned char val)
{
    int i;
    unsigned char result = 0;   // init to make compiler happy

    for (i = 0; i < 8; i++) {
        result = (result << 1) + (val & 0x01);
        val >>= 1;
    }

    return result;
}

#if 0
/*
 * Flush any remaining bits out.  Call this at the very end.
 */
void
BitBuffer::FlushBits(void)
{
    if (fBitCount) {
        fBits <<= 8 - fBitCount;
        putc(fBits, fFp);

        fBitCount = 0;
    }
}
#endif


/*
 * Compute the #of times each byte appears in trackBuf.  Runs of four
 * bytes or longer are completely ignored.
 *
 * "trackBuf" holds kTrackLen bytes of data, and "freqCounts" holds
 * kNumSymbols (256) unsigned shorts.
 */
void
ComputeFreqCounts(const unsigned char* trackBuf, unsigned short* freqCounts)
{
    const unsigned char* ucp;
    int i;

    memset(freqCounts, 0, 256 * sizeof(unsigned short));

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

            while (*ucp == *(ucp+1) && i < kTrackLen) {
                runLen++;
                ucp++;
                i++;

                if (runLen == 256) {
                    runLen = 0;
                    break;
                }
            }

            //printf("Found run of %d of 0x%02x\n", runLen, *ucp);
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
void
ComputeFavorites(unsigned short* freqCounts, unsigned char* favorites)
{
    int i, fav;

    for (fav = 0; fav < kNumFavorites; fav++) {
        unsigned short bestCount = 0;
        unsigned char bestSym = 0;

        for (i = 0; i < kNumSymbols; i++) {
            if (freqCounts[i] >= bestCount) {
                bestSym = (unsigned char) i;
                bestCount = freqCounts[i];
            }
        }

        favorites[fav] = bestSym;
        freqCounts[bestSym] = 0;
    }

    //printf("FAVORITES: ");
    //for (fav = 0; fav < kNumFavorites; fav++)
    //    printf("%02x ", favorites[fav]);
    //printf("\n");
}

/*
 * These are all odd, which when they're written in reverse order means
 * they all have their hi bits set.
 */
static const unsigned char kFavoriteBitEnc[kNumFavorites] = {
    0x03, 0x09, 0x1f, 0x0f, 0x07, 0x1b, 0x0b, 0x0d, 0x15, 0x37,
    0x3d, 0x25, 0x05, 0xb1, 0x11, 0x21, 0x01, 0x57, 0x5d, 0x1d
};
static const int kFavoriteBitEncLen[kNumFavorites] = {
    4, 4, 5, 5, 5, 5, 5, 5, 5, 6,
    6, 6, 6, 6, 6, 6, 6, 7, 7, 7
};


/*
 * Compress a track full of data.
 */
void
CompressTrack(const unsigned char* trackBuf, BitBuffer* pBitBuf)
{
    unsigned short freqCounts[kNumSymbols];
    unsigned char favorites[kNumFavorites];
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
    const unsigned char* ucp = trackBuf;
    for (i = 0; i < kTrackLen; i++, ucp++) {
        if (i < (kTrackLen-3) &&
            *ucp == *(ucp+1) &&
            *ucp == *(ucp+2) &&
            *ucp == *(ucp+3))
        {
            int runLen = 4;
            i += 3;
            ucp += 3;

            while (*ucp == *(ucp+1) && i < kTrackLen) {
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
 * Handle a debug message from the DiskImg library.
 */
/*static*/ void
MsgHandler(const char* file, int line, const char* msg)
{
    assert(file != nil);
    assert(msg != nil);

    fprintf(gLog, "%05u %s", gPid, msg);
}
/*
 * Handle a global error message from the NufxLib library by shoving it
 * through the DiskImgLib message function.
 */
NuResult
NufxErrorMsgHandler(NuArchive* /*pArchive*/, void* vErrorMessage)
{
    const NuErrorMessage* pErrorMessage = (const NuErrorMessage*) vErrorMessage;

    if (pErrorMessage->isDebug) {
        Global::PrintDebugMsg(pErrorMessage->file, pErrorMessage->line,
            "<nufxlib> [D] %s\n", pErrorMessage->message);
    } else {
        Global::PrintDebugMsg(pErrorMessage->file, pErrorMessage->line,
            "<nufxlib> %s\n", pErrorMessage->message);
    }

    return kNuOK;
}

/*
 * Pack a disk image with DDD.
 */
DIError
Pack(const char* infile, const char* outfile)
{
    DIError dierr = kDIErrNone;
    DiskImg srcImg;
    FILE* outfp = nil;
    BitBuffer bitBuffer;

    printf("Packing in='%s' out='%s'\n", infile, outfile);

    /*
     * Prepare the source image.
     */
    dierr = srcImg.OpenImage(infile, '/', true);
    if (dierr != kDIErrNone) {
        fprintf(stderr, "Unable to open disk image: %s.\n",
            DiskImgLib::DIStrError(dierr));
        goto bail;
    }

    dierr = srcImg.AnalyzeImage();
    if (dierr != kDIErrNone) {
        fprintf(stderr, "Unable to determine source image format.\n");
        goto bail;
    }

    if (!srcImg.GetHasSectors()) {
        fprintf(stderr, "Sorry, only sector-addressable images allowed.\n");
        dierr = kDIErrUnsupportedPhysicalFmt;
        goto bail;
    }
    assert(srcImg.GetNumSectPerTrack() > 0);

    if (srcImg.GetSectorOrder() == DiskImg::kSectorOrderUnknown) {
        fprintf(stderr, "(QUERY) don't know sector order\n");
        dierr = kDIErrFilesystemNotFound;
        goto bail;
    }

    /* force the access to be DOS-ordered */
    dierr = srcImg.OverrideFormat(srcImg.GetPhysicalFormat(),
                DiskImg::kFormatGenericDOSOrd, srcImg.GetSectorOrder());
    if (dierr != kDIErrNone) {
        fprintf(stderr, "Couldn't switch to generic ProDOS: %s.\n",
            DiskImgLib::DIStrError(dierr));
        goto bail;
    }

    /* transfer the DOS volume num, if one was set */
    printf("DOS volume number set to %d\n", srcImg.GetDOSVolumeNum());
    if (srcImg.GetDOSVolumeNum() == DiskImg::kVolumeNumNotSet)
        srcImg.SetDOSVolumeNum(kDefaultNibbleVolumeNum);

    /*
     * Open the output file.
     */
    outfp = fopen(outfile, "w");
    if (outfp == nil) {
        perror("unable to open output file");
        dierr = kDIErrGeneric;
        goto bail;
    }

    /* write four zeroes to replace the DOS addr/len bytes */
    /* (let's write the apparent DDD Pro v1.1 signature instead) */
    putc(kDDDProSignature, outfp);
    putc(kDDDProSignature >> 8, outfp);
    putc(kDDDProSignature >> 16, outfp);
    putc(kDDDProSignature >> 24, outfp);

    bitBuffer.SetFile(outfp);

    bitBuffer.PutBits(0x00, 3);
    bitBuffer.PutBits(srcImg.GetDOSVolumeNum(), 8);

    /*
     * Process all tracks.
     */
    for (int track = 0; track < srcImg.GetNumTracks(); track++) {
        unsigned char trackBuf[kTrackLen];

        /*
         * Read the track.
         */
        for (int sector = 0; sector < srcImg.GetNumSectPerTrack(); sector++) {
            dierr = srcImg.ReadTrackSector(track, sector,
                        trackBuf + sector * 256);
            if (dierr != kDIErrNone) {
                fprintf(stderr, "ERROR: ReadBlock failed (err=%d)\n", dierr);
                goto bail;
            }
        }

        //printf("Got track %d (0x%02x %02x %02x %02x %02x %02x ...)\n",
        //    track, trackBuf[0], trackBuf[1], trackBuf[2], trackBuf[3],
        //    trackBuf[4], trackBuf[5]);
        CompressTrack(trackBuf, &bitBuffer);
    }

    /* write 8 bits of zeroes to flush remaining data out of buffer */
    bitBuffer.PutBits(0x00, 8);

    /* write another zero byte because that's what DDD Pro v1.1 does */
    long zero;
    zero = 0;
    fwrite(&zero, 1, 1, outfp);

    dierr = srcImg.CloseImage();
    if (dierr != kDIErrNone) {
        fprintf(stderr, "ERROR: srcImg close failed?!\n");
        goto bail;
    }

    assert(dierr == kDIErrNone);
bail:
    return dierr;
}

/*
 * This is the reverse of the kFavoriteBitEnc table.  The bits are
 * reversed and lack the high bit.
 */
static const unsigned char kFavoriteBitDec[kNumFavorites] = {
    0x04, 0x01, 0x0f, 0x0e, 0x0c, 0x0b, 0x0a, 0x06, 0x05, 0x1b,
    0x0f, 0x09, 0x08, 0x03, 0x02, 0x01, 0x00, 0x35, 0x1d, 0x1c
};

/*
 * Unpack a single track.
 *
 * Returns "true" if all went well, "false" if something failed.
 */
bool
UnpackTrack(BitBuffer* pBitBuffer, unsigned char* trackBuf)
{
    unsigned char favorites[kNumFavorites];
    unsigned char val;
    unsigned char* trackPtr;
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
                unsigned char rleChar;
                int rleCount;

                (void) pBitBuffer->GetBits(1);  // get last bit of 0x97
                val = pBitBuffer->GetBits(8);
                rleChar = pBitBuffer->Reverse(val);
                val = pBitBuffer->GetBits(8);
                rleCount = pBitBuffer->Reverse(val);
                printf("Found run of %d of 0x%02x\n", rleCount, rleChar);

                if (rleCount == 0)
                    rleCount = 256;

                /* make sure we won't overrun */
                if (trackPtr + rleCount > trackBuf + kTrackLen) {
                    printf("Overrun in RLE\n");
                    return false;
                }
                while (rleCount--)
                    *trackPtr++ = rleChar;
            }
        }
    }

    return true;
}

/*
 * Unpack a disk image compressed with DDD.
 *
 * The result is an unadorned DOS-ordered image.
 */
DIError
Unpack(const char* infile, const char* outfile)
{
    DIError dierr = kDIErrNone;
    FILE* infp = nil;
    FILE* outfp = nil;
    BitBuffer bitBuffer;
    unsigned char val;

    printf("Unpacking in='%s' out='%s'\n", infile, outfile);

    /*
     * Open the input file.
     */
    infp = fopen(infile, "r");
    if (infp == nil) {
        perror("unable to open input file");
        dierr = kDIErrGeneric;
        goto bail;
    }

    /*
     * Open the output file.
     */
    outfp = fopen(outfile, "w");
    if (outfp == nil) {
        perror("unable to open output file");
        dierr = kDIErrGeneric;
        goto bail;
    }

    /* read four zeroes to skip the DOS addr/len bytes */
    (void) getc(infp);
    (void) getc(infp);
    (void) getc(infp);
    (void) getc(infp);

    bitBuffer.SetFile(infp);

    val = bitBuffer.GetBits(3);
    if (val != 0) {
        printf("HEY: this isn't a DDD II file (%d)\n", val);
        dierr = kDIErrGeneric;
        goto bail;
    }
    val = bitBuffer.GetBits(8);
    val = bitBuffer.Reverse(val);
    printf("GOT disk volume num = %d\n", val);

    for (int track = 0; track < kNumTracks; track++) {
        unsigned char trackBuf[kTrackLen];

        if (!UnpackTrack(&bitBuffer, trackBuf)) {
            fprintf(stderr, "FAILED on track %d\n", track);
            dierr = kDIErrBadCompressedData;
            goto bail;
        }
        if (feof(infp) || ferror(infp)) {
            fprintf(stderr, "Failure or EOF on input file\n");
            dierr = kDIErrBadCompressedData;
            goto bail;
        }
        fwrite(trackBuf, 1, 4096, outfp);
    }

    /*
     * We should be within a byte or two of the end of the file.
     */
    (void) getc(infp);
    (void) getc(infp);
    (void) getc(infp);
    (void) getc(infp);
    if (!feof(infp)) {
        fprintf(stderr, "Looks like too much data in input file\n");
        dierr = kDIErrBadCompressedData;
        goto bail;
    }

    assert(dierr == kDIErrNone);
bail:
    return dierr;
}

/*
 * Process every argument.
 */
int
main(int argc, char** argv)
{
//    const char* kLogFile = "iconv-log.txt";

    if (argc != 3) {
        fprintf(stderr, "%s: infile outfile\n", argv[0]);
        exit(2);
    }

    gLog = stdout;
//    gLog = fopen(kLogFile, "w");
//    if (gLog == nil) {
//        fprintf(stderr, "ERROR: unable to open log file\n");
//        exit(1);
//    }

    printf("DDD Converter for Linux v1.0\n");
    printf("Copyright (C) 2003 by faddenSoft, LLC.  All rights reserved.\n");
    int32_t major, minor, bug;
    Global::GetVersion(&major, &minor, &bug);
    printf("Linked against DiskImg library v%d.%d.%d\n",
        major, minor, bug);
//    printf("Log file is '%s'\n", kLogFile);
    printf("\n");

    Global::SetDebugMsgHandler(MsgHandler);
    Global::AppInit();

    NuSetGlobalErrorMessageHandler(NufxErrorMsgHandler);

    int len = strlen(argv[2]);
    if (len > 3 && strcasecmp(argv[2] + len - 3, ".do") == 0) {
        Unpack(argv[1], argv[2]);
    } else {
        Pack(argv[1], argv[2]);
    }

    Global::AppCleanup();
    fclose(gLog);

    exit(0);
}

