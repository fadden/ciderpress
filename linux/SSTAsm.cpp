/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Reassemble SST disk images into a .NIB file.
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <time.h>
#include "../diskimg/DiskImg.h"

using namespace DiskImgLib;

#define nil NULL


#if 0
inline int
ConvOddEven(unsigned char val1, unsigned char val2)
{
    return ((val1 & 0x55) << 1) | (val2 & 0x55);
}
#endif


const int kSSTNumTracks = 35;
const int kSSTTrackLen = 6656;     // or 6384 for .NB2

/*
 * Compute the destination file offset for a particular source track.  The
 * track number ranges from 0 to 69 inclusive.  Sectors from two adjacent
 * "cooked" tracks are combined into a single "raw nibbilized" track.
 *
 * The data is ordered like this:
 *  track 1 sector 15 --> track 1 sector 4  (12 sectors)
 *  track 0 sector 13 --> track 0 sector 0  (14 sectors)
 *
 * Total of 26 sectors, or $1a00 bytes.
 */
long
GetBufOffset(int track)
{
    assert(track >= 0 && track < kSSTNumTracks*2);

    long offset;

    if (track & 0x01) {
        /* odd, use start of data */
        offset = (track / 2) * kSSTTrackLen;
    } else {
        /* even, start of data plus 12 sectors */
        offset = (track / 2) * kSSTTrackLen + 12 * 256;
    }

    assert(offset >= 0 && offset < kSSTTrackLen * kSSTNumTracks);

    return offset;
}

/*
 * Copy 17.5 tracks of data from the SST image to a .NIB image.
 *
 * Data is stored in all 16 sectors of track 0, followed by the first
 * 12 sectors of track 1, then on to track 2.  Total of $1a00 bytes.
 */
int
LoadSSTData(DiskImg* pDiskImg, int seqNum, unsigned char* trackBuf)
{
    DIError dierr;
    char sctBuf[256];
    int track, sector;
    long bufOffset;

    for (track = 0; track < kSSTNumTracks; track++) {
        int virtualTrack = track + (seqNum * kSSTNumTracks);
        bufOffset = GetBufOffset(virtualTrack);
        //fprintf(stderr, "USING offset=%ld (track=%d / %d)\n",
        //    bufOffset, track, virtualTrack);

        if (virtualTrack & 0x01) {
            /* odd-numbered track, sectors 15-4 */
            for (sector = 15; sector >= 4; sector--) {
                dierr = pDiskImg->ReadTrackSector(track, sector, sctBuf);
                if (dierr != kDIErrNone) {
                    fprintf(stderr, "ERROR: on track=%d sector=%d\n",
                        track, sector);
                    return -1;
                }

                memcpy(trackBuf + bufOffset, sctBuf, 256);
                bufOffset += 256;
            }
        } else {
            for (sector = 13; sector >= 0; sector--) {
                dierr = pDiskImg->ReadTrackSector(track, sector, sctBuf);
                if (dierr != kDIErrNone) {
                    fprintf(stderr, "ERROR: on track=%d sector=%d\n",
                        track, sector);
                    return -1;
                }

                memcpy(trackBuf + bufOffset, sctBuf, 256);
                bufOffset += 256;
            }
        }
    }

#if 0
    int i;
    for (i = 0; (size_t) i < sizeof(trackBuf)-10; i++) {
        if ((trackBuf[i] | 0x80) == 0xd5 &&
            (trackBuf[i+1] | 0x80) == 0xaa &&
            (trackBuf[i+2] | 0x80) == 0x96)
        {
            fprintf(stderr, "off=%5d vol=%d trk=%d sct=%d chk=%d\n", i,
                ConvOddEven(trackBuf[i+3], trackBuf[i+4]),
                ConvOddEven(trackBuf[i+5], trackBuf[i+6]),
                ConvOddEven(trackBuf[i+7], trackBuf[i+8]),
                ConvOddEven(trackBuf[i+9], trackBuf[i+10]));
            i += 10;
            if ((size_t)i < sizeof(trackBuf)-3) {
                fprintf(stderr, "  0x%02x 0x%02x 0x%02x\n",
                    trackBuf[i+1], trackBuf[i+2], trackBuf[i+3]);
            }
        }
    }
#endif

    return 0;
}

/*
 * Copy sectors from a single image.
 */
int
HandleSSTImage(const char* fileName, int seqNum, unsigned char* trackBuf)
{
    DIError dierr;
    DiskImg diskImg;
    int result = -1;

    fprintf(stderr, "Handling '%s'\n", fileName);

    dierr = diskImg.OpenImage(fileName, '/', true);
    if (dierr != kDIErrNone) {
        fprintf(stderr, "ERROR: unable to open '%s'\n", fileName);
        goto bail;
    }

    dierr = diskImg.AnalyzeImage();
    if (dierr != kDIErrNone) {
        fprintf(stderr, "ERROR: image analysis failed\n");
        goto bail;
    }

    if (diskImg.GetSectorOrder() == DiskImg::kSectorOrderUnknown) {
        fprintf(stderr, "ERROR: sector order not set\n");
        goto bail;
    }
    if (diskImg.GetFSFormat() != DiskImg::kFormatUnknown) {
        fprintf(stderr, "WARNING: file format *was* recognized!\n");
        goto bail;
    }
    if (diskImg.GetNumTracks() != kSSTNumTracks ||
        diskImg.GetNumSectPerTrack() != 16)
    {
        fprintf(stderr, "ERROR: only 140K floppies can be SST inputs\n");
        goto bail;
    }

    dierr = diskImg.OverrideFormat(diskImg.GetPhysicalFormat(),
                DiskImg::kFormatGenericDOSOrd, diskImg.GetSectorOrder());
    if (dierr != kDIErrNone) {
        fprintf(stderr, "ERROR: format override failed\n");
        goto bail;
    }

    /*
     * We have the image open successfully, now do something with it.
     */
    result = LoadSSTData(&diskImg, seqNum, trackBuf);

bail:
    return result;
}

/*
 * Run through the data, adding 0x80 everywhere and re-aligning the
 * tracks so that the big clump of sync bytes is at the end.
 */
int
ProcessTrackData(unsigned char* trackBuf)
{
    unsigned char* trackPtr;
    int track;

    for (track = 0, trackPtr = trackBuf;  track < kSSTNumTracks;
        track++, trackPtr += kSSTTrackLen)
    {
        bool inRun;
        int start = 0;
        int longestStart = -1;
        int count7f = 0;
        int longest = -1;
        int i;

        inRun = false;
        for (i = 0; i < kSSTTrackLen; i++) {
            if (trackPtr[i] == 0x7f) {
                if (inRun) {
                    count7f++;
                } else {
                    count7f = 1;
                    start = i;
                    inRun = true;
                }
            } else {
                if (inRun) {
                    if (count7f > longest) {
                        longest = count7f;
                        longestStart = start;
                    }
                    inRun = false;
                } else {
                    /* do nothing */
                }
            }

            trackPtr[i] |= 0x80;
        }


        if (longest == -1) {
            fprintf(stderr, "HEY: couldn't find any 0x7f in track %d\n",
                track);
        } else {
            fprintf(stderr, "Found run of %d at %d in track %d\n",
                longest, longestStart, track);

            int bkpt = longestStart + longest;
            assert(bkpt < kSSTTrackLen);

            char oneTrack[kSSTTrackLen];
            memcpy(oneTrack, trackPtr, kSSTTrackLen);

            /* copy it back so sync bytes are at end of track */
            memcpy(trackPtr, oneTrack + bkpt, kSSTTrackLen - bkpt);
            memcpy(trackPtr + (kSSTTrackLen - bkpt), oneTrack, bkpt);
        }
    }
    
    return 0;
}

/*
 * Read sectors from file1 and file2, and write them in the correct
 * sequence to outfp.
 */
int
ReassembleSST(const char* file1, const char* file2, FILE* outfp)
{
    unsigned char* trackBuf = nil;
    int result;

    trackBuf = new unsigned char[kSSTNumTracks * kSSTTrackLen];
    if (trackBuf == nil) {
        fprintf(stderr, "ERROR: malloc failed\n");
        return -1;
    }

    result = HandleSSTImage(file1, 0, trackBuf);
    if (result != 0)
        return result;

    result = HandleSSTImage(file2, 1, trackBuf);
    if (result != 0)
        return result;

    result = ProcessTrackData(trackBuf);

    fprintf(stderr, "Writing %d bytes\n", kSSTNumTracks * kSSTTrackLen);
    fwrite(trackBuf, 1, kSSTNumTracks * kSSTTrackLen, outfp);

    delete[] trackBuf;
    return result;
}


/*
 * Handle a debug message from the DiskImg library.
 */
/*static*/ void
MsgHandler(const char* file, int line, const char* msg)
{
    assert(file != nil);
    assert(msg != nil);

    fprintf(stderr, "%s", msg);
}

/*
 * Parse args, go.
 */
int
main(int argc, char** argv)
{
    int result;

    if (argc != 3) {
        fprintf(stderr, "Usage: %s file1 file2 > outfile\n", argv[0]);
        exit(2);
    }

    Global::SetDebugMsgHandler(MsgHandler);
    Global::AppInit();

    result = ReassembleSST(argv[1], argv[2], stdout);

    Global::AppCleanup();
    exit(result != 0);
}

