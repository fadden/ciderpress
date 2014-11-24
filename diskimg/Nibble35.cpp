/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * GCR nibble image support for 3.5" disks.
 *
 * Each track has between 8 and 12 512-byte sectors.  The encoding is similar
 * to but different from that used on 5.25" disks.
 *
 * THOUGHT: this is currently designed for unpacking all blocks from a track.
 * We really ought to allow the user to view the track in nibble form, which
 * means reworking the interface to be more like the 5.25" nibble stuff.  We
 * should present it as a block interface rather than track/sector; the code
 * here can convert the block # to track/sector, and just provide a raw
 * interface for the nibble track viewer.
 */
#include "StdAfx.h"
#include "DiskImgPriv.h"

/*
Physical sector layout:

+00  self-sync 0xff pattern (36 10-bit bytes, or 45 8-bit bytes)
+36  addr prolog (0xd5 0xaa 0x96)
+39  6&2enc track number (0-79 mod 63)
+40  6&2enc sector number (0-N)
+41  6&2enc side (0x00 or 0x20, ORed with 0x01 for tracks >= 64)
+42  6&2enc format (0x22, 0x24, others?)
+43  6&2enc checksum (track ^ sector ^ side ^ format)
+44  addr epilog (0xde 0xaa)
+46  self-sync 0xff (6 10-bit bytes)
+52  data prolog (0xd5 0xaa 0xad)
+55  6&2enc sector number (another copy)
+56  6&2enc nibblized data (699 bytes)
+755 checksum, 3 bytes 6&2 encoded as 4 bytes
+759 data epilog (0xde 0xaa)
+761 0xff (pad byte)

Some sources say it starts with 42 10-bit self-sync bytes instead of 36.
*/

/*
 * Basic disk geometry.
 */
const int kCylindersPerDisk = 80;
const int kHeadsPerCylinder = 2;
const int kMaxSectorsPerTrack = 12;
const int kSectorSize35 = 524;      // 512 data bytes + 12 tag bytes
const int kTagBytesLen = 12;
const int kDataChecksumLen = 3;
const int kChunkSize35 = 175;       // ceil(524 / 3)
const int kOffsetToChecksum = 699;
const int kNibblizedOutputLen = (kOffsetToChecksum + 4);
const int kMaxDataReach = 48;       // should only be 6 bytes */

enum {
    kAddrProlog0 = 0xd5,
    kAddrProlog1 = 0xaa,
    kAddrProlog2 = 0x96,
    kAddrEpilog0 = 0xde,
    kAddrEpilog1 = 0xaa,

    kDataProlog0 = 0xd5,
    kDataProlog1 = 0xaa,
    kDataProlog2 = 0xad,
    kDataEpilog0 = 0xde,
    kDataEpilog1 = 0xaa,
};

/*
 * There are 12 sectors per track for the first 16 cylinders, 11 sectors
 * per track for the next 16, and so on until we're down to 8 per track.
 */
/*static*/ int DiskImg::SectorsPerTrack35(int cylinder)
{
    return kMaxSectorsPerTrack - (cylinder / 16);
}

/*
 * Convert cylinder/head/sector to a block number on a 3.5" disk.
 */
/*static*/ int DiskImg::CylHeadSect35ToBlock(int cyl, int head, int sect)
{
    int i, block;

    assert(cyl >= 0 && cyl < kCylindersPerDisk);
    assert(head >= 0 && head < kHeadsPerCylinder);
    assert(sect >= 0 && sect < SectorsPerTrack35(cyl));

    block = 0;
    for (i = 0; i < cyl; i++)
        block += SectorsPerTrack35(i) * kHeadsPerCylinder;
    if (head)
        block += SectorsPerTrack35(i);
    block += sect;

    //LOGI("Nib35: c/h/s %d/%d/%d --> block %d", cyl, head, sect, block);
    assert(block >= 0 && block < 1600);
    return block;
}

/*
 * Unpack a nibble track.
 *
 * "outputBuf" must be able to hold 512 * 12 sectors of decoded sector data.
 */
/*static*/ DIError DiskImg::UnpackNibbleTrack35(const uint8_t* nibbleBuf,
    long nibbleLen, uint8_t* outputBuf, int cyl, int head,
    LinearBitmap* pBadBlockMap)
{
    CircularBufferAccess buffer(nibbleBuf, nibbleLen);
    bool foundSector[kMaxSectorsPerTrack];
    uint8_t sectorBuf[kSectorSize35];
    uint8_t readSum[kDataChecksumLen];
    uint8_t calcSum[kDataChecksumLen];
    int i;

    memset(&foundSector, 0, sizeof(foundSector));

    i = 0;
    while (i < nibbleLen) {
        int sector;

        i = FindNextSector35(buffer, i, cyl, head, &sector);
        if (i < 0)
            break;

        assert(sector >= 0 && sector < SectorsPerTrack35(cyl));
        if (foundSector[sector]) {
            LOGI("Nib35: WARNING: found two copies of sect %d on cyl=%d head=%d",
                sector, cyl, head);
        } else {
            memset(sectorBuf, 0xa9, sizeof(sectorBuf));
            if (DecodeNibbleSector35(buffer, i, sectorBuf, readSum, calcSum))
            {
                /* successfully decoded sector, copy data & verify checksum */
                foundSector[sector] = true;
                memcpy(outputBuf + kBlockSize * sector,
                    sectorBuf + kTagBytesLen, kBlockSize);

                if (calcSum[0] != readSum[0] ||
                    calcSum[1] != readSum[1] ||
                    calcSum[2] != readSum[2])
                {
                    LOGI("Nib35: checksum mismatch: 0x%06x vs. 0x%06x",
                        calcSum[0] << 16 | calcSum[1] << 8 | calcSum[2],
                        readSum[0] << 16 | readSum[1] << 8 | readSum[2]);
                    LOGI("Nib35:  marking cyl=%d head=%d sect=%d (block=%d)",
                        cyl, head, sector,
                        CylHeadSect35ToBlock(cyl, head, sector));
                    pBadBlockMap->Set(CylHeadSect35ToBlock(cyl, head, sector));
                }
            }
        }
    }

    /*
     * Check to see if we have all our parts.  Anything missing sets
     * a flag in the "bad block" map.
     */
    for (i = SectorsPerTrack35(cyl)-1; i >= 0; i--) {
        if (!foundSector[i]) {
            LOGI("Nib35: didn't find cyl=%d head=%d sect=%d (block=%d)",
                cyl, head, i, CylHeadSect35ToBlock(cyl, head, i));
            pBadBlockMap->Set(CylHeadSect35ToBlock(cyl, head, i));
        }

        /*
        // DEBUG test
        if ((cyl == 0 || cyl == 12 || cyl == 79) &&
            (head == (cyl & 0x01)) &&
            (i == 1 || i == 7))
        {
            LOGI("DEBUG: setting bad %d/%d/%d (%d)",
                cyl, head, i, CylHeadSect35ToBlock(cyl, head, i));
            pBadBlockMap->Set(CylHeadSect35ToBlock(cyl, head, i));
        }
        */
    }

    return kDIErrNone;      // maybe return an error if nothing found?
}

/*
 * Returns the offset of the next sector, or -1 if we went off the end.
 */
/*static*/ int DiskImg::FindNextSector35(const CircularBufferAccess& buffer,
    int start, int cyl, int head, int* pSector)
{
    int end = buffer.GetSize();
    int i;

    for (i = start; i < end; i++) {
        bool foundAddr = false;

        if (buffer[i] == kAddrProlog0 &&
            buffer[i+1] == kAddrProlog1 &&
            buffer[i+2] == kAddrProlog2)
        {
            foundAddr = true;
        }

        if (foundAddr) {
            /* decode the address field */
            int trackNum, sectNum, side, format, checksum;

            trackNum = kInvDiskBytes62[buffer[i+3]];
            sectNum = kInvDiskBytes62[buffer[i+4]];
            side = kInvDiskBytes62[buffer[i+5]];
            format = kInvDiskBytes62[buffer[i+6]];
            checksum = kInvDiskBytes62[buffer[i+7]];
            if (trackNum == kInvInvalidValue ||
                sectNum == kInvInvalidValue ||
                side == kInvInvalidValue ||
                format == kInvInvalidValue ||
                checksum == kInvInvalidValue)
            {
                LOGI("Nib35: garbled address header found");
                continue;
            }
            //LOGI(" Nib35: got addr: track=%2d sect=%2d side=%d format=%d sum=0x%02x",
            //  trackNum, sectNum, side, format, checksum);
            if (side != ((head * 0x20) | (cyl >> 6))) {
                LOGI("Nib35: unexpected value for side: %d on cyl=%d head=%d",
                    side, cyl, head);
            }
            if (sectNum >= SectorsPerTrack35(cyl)) {
                LOGI("Nib35: invalid value for sector: %d (cyl=%d)",
                    sectNum, cyl);
                continue;
            }
            /* format seems to be 0x22 or 0x24 */
            if (checksum != (trackNum ^ sectNum ^ side ^ format)) {
                LOGI("Nib35: unexpected checksum: 0x%02x vs. 0x%02x",
                    checksum, trackNum ^ sectNum ^ side ^ format);
                continue;
            }

            /* check the epilog bytes */
            if (buffer[i+8] != kAddrEpilog0 ||
                buffer[i+9] != kAddrEpilog1)
            {
                LOGI("Nib35: invalid address epilog");
                /* maybe we allow this anyway? */
            }

            *pSector = sectNum;
            return i+10;        // move past address field
        }
    }

    return -1;
}

/*
 * Unpack a 524-byte sector from a 3.5" disk.  Start with "start" pointed
 * in the general vicinity of the data prolog bytes.
 *
 * "sectorBuf" must hold at least kSectorSize35 bytes.  It will be filled
 * with the decoded data.
 * "readChecksum" and "calcChecksum" must each hold at least kDataChecksumLen
 * bytes.  The former holds the checksum read from the sector, the latter
 * holds the checksum computed from the data.
 *
 * The 4 to 3 conversion is pretty straightforward.  The checksum is
 * a little crazy.
 *
 * Returns "true" if all goes well, "false" if there is a problem.  Does
 * not return false on a checksum mismatch -- it's up to the caller to
 * verify the checksum if desired.
 */
/*static*/ bool DiskImg::DecodeNibbleSector35(const CircularBufferAccess& buffer,
    int start, uint8_t* sectorBuf, uint8_t* readChecksum,
    uint8_t* calcChecksum)
{
    const int kMaxDataReach35 = 48;       // fairly arbitrary
    uint8_t* sectorBufStart = sectorBuf;
    uint8_t part0[kChunkSize35], part1[kChunkSize35], part2[kChunkSize35];
    unsigned int chk0, chk1, chk2;
    uint8_t val, nib0, nib1, nib2, twos;
    int i, off;

    /*
     * Find the start of the actual data.  Adjust "start" to point at it.
     */
    for (off = start; off < start + kMaxDataReach35; off++) {
        if (buffer[off] == kDataProlog0 &&
            buffer[off+1] == kDataProlog1 &&
            buffer[off+2] == kDataProlog2)
        {
            start = off + 4;    // 3 prolog bytes + sector number
            break;
        }
    }
    if (off == start + kMaxDataReach35) {
        LOGI("nib25: could not find start of data field");
        return false;
    }

    /*
     * Assemble 8-bit bytes from 6&2 encoded values.
     */
    off = start;
    for (i = 0; i < kChunkSize35; i++) {
        twos = kInvDiskBytes62[buffer[off++]];
        nib0 = kInvDiskBytes62[buffer[off++]];
        nib1 = kInvDiskBytes62[buffer[off++]];
        if (i != kChunkSize35-1)
            nib2 = kInvDiskBytes62[buffer[off++]];
        else
            nib2 = 0;

        if (twos == kInvInvalidValue ||
            nib0 == kInvInvalidValue ||
            nib1 == kInvInvalidValue ||
            nib2 == kInvInvalidValue)
        {
            // junk found
            LOGI("Nib25: found invalid disk byte in sector data at %d",
                off - start);
            LOGI("       (one of 0x%02x 0x%02x 0x%02x 0x%02x)",
                buffer[off-4], buffer[off-3], buffer[off-2], buffer[off-1]);
            return false;
            //if (twos == kInvInvalidValue)
            //  twos = 0;
            //if (nib0 == kInvInvalidValue)
            //  nib0 = 0;
            //if (nib1 == kInvInvalidValue)
            //  nib1 = 0;
            //if (nib2 == kInvInvalidValue)
            //  nib2 = 0;
        }

        part0[i] = nib0 | ((twos << 2) & 0xc0);
        part1[i] = nib1 | ((twos << 4) & 0xc0);
        part2[i] = nib2 | ((twos << 6) & 0xc0);
    }
    assert(off == start + kOffsetToChecksum);

    chk0 = chk1 = chk2 = 0;
    i = 0;
    while (true) {
        chk0 = (chk0 & 0xff) << 1;
        if (chk0 & 0x0100)
            chk0++;

        val = part0[i] ^ chk0;
        chk2 += val;
        if (chk0 & 0x0100) {
            chk2++;
            chk0 &= 0xff;
        }
        *sectorBuf++ = val;

        val = part1[i] ^ chk2;
        chk1 += val;
        if (chk2 > 0xff) {
            chk1++;
            chk2 &= 0xff;
        }
        *sectorBuf++ = val;

        if (sectorBuf - sectorBufStart == 524)
            break;

        val = part2[i] ^ chk1;
        chk0 += val;
        if (chk1 > 0xff) {
            chk0++;
            chk1 &= 0xff;
        }
        *sectorBuf++ = val;

        i++;
        assert(i < kChunkSize35);
        //LOGI("i = %d, diff=%d", i, sectorBuf - sectorBufStart);
    }

    calcChecksum[0] = chk0;
    calcChecksum[1] = chk1;
    calcChecksum[2] = chk2;

    if (!UnpackChecksum35(buffer, off, readChecksum)) {
        LOGI("Nib35: failure reading checksum");
        readChecksum[0] = calcChecksum[0] ^ 0xff;   // force a failure
        return false;
    }
    off += 4;       // skip past checksum bytes

    if (buffer[off] != kDataEpilog0 || buffer[off+1] != kDataEpilog1) {
        LOGI("nib25: WARNING: data epilog not found");
        // allow it, if the checksum matches
    }

//#define TEST_ENC_35
#ifdef TEST_ENC_35
    {
        uint8_t nibBuf[kNibblizedOutputLen];
        memset(nibBuf, 0xcc, sizeof(nibBuf));

        /* encode what we just decoded */
        EncodeNibbleSector35(sectorBufStart, nibBuf);
        /* compare it to the original */
        for (i = 0; i < kNibblizedOutputLen; i++) {
            if (buffer[start + i] != nibBuf[i]) {
                /*
                 * The very last "twos" entry may have undefined bits when
                 * written by a real drive.  Peel it apart and ignore the
                 * two flaky bits.
                 */
                if (i == 696) {
                    uint8_t val1, val2;
                    val1 = kInvDiskBytes62[buffer[start + i]];
                    val2 = kInvDiskBytes62[nibBuf[i]];
                    if ((val1 & 0xfc) != (val2 & 0xfc)) {
                        LOGI("Nib35 DEBUG: output differs at byte %d"
                              " (0x%02x vs 0x%02x / 0x%02x vs 0x%02x)",
                            i, buffer[start+i], nibBuf[i], val1, val2);
                    }
                } else {
                    // note: checksum is 699-702
                    LOGI("Nib35 DEBUG: output differs at byte %d (0x%02x vs 0x%02x)",
                        i, buffer[start+i], nibBuf[i]);
                }
            }
        }
    }
#endif /*TEST_ENC_35*/

    return true;
}

/*
 * Unpack the 6&2 encoded 3-byte checksum at the end of a sector.
 *
 * "offset" should point to the first byte of the checksum.
 *
 * Returns "true" if all goes well, "false" otherwise.
 */
/*static*/ bool DiskImg::UnpackChecksum35(const CircularBufferAccess& buffer,
    int offset, uint8_t* checksumBuf)
{
    uint8_t nib0, nib1, nib2, twos;
    
    twos = kInvDiskBytes62[buffer[offset++]];
    nib2 = kInvDiskBytes62[buffer[offset++]];
    nib1 = kInvDiskBytes62[buffer[offset++]];
    nib0 = kInvDiskBytes62[buffer[offset++]];

    if (twos == kInvInvalidValue ||
        nib0 == kInvInvalidValue ||
        nib1 == kInvInvalidValue ||
        nib2 == kInvInvalidValue)
    {
        LOGI("nib25: found invalid disk byte in checksum");
        return false;
    }

    checksumBuf[0] = nib0 | ((twos << 6) & 0xc0);
    checksumBuf[1] = nib1 | ((twos << 4) & 0xc0);
    checksumBuf[2] = nib2 | ((twos << 2) & 0xc0);
    return true;
}

/*
 * Encode 524 bytes of sector data into 699 bytes of 6&2 nibblized data
 * plus a 4-byte checksum.
 *
 * "outBuf" must be able to hold kNibblizedOutputLen bytes.
 */
/*static*/ void DiskImg::EncodeNibbleSector35(const uint8_t* sectorData,
    uint8_t* outBuf)
{
    const uint8_t* sectorDataStart = sectorData;
    uint8_t* outBufStart = outBuf;
    uint8_t part0[kChunkSize35], part1[kChunkSize35], part2[kChunkSize35];
    unsigned int chk0, chk1, chk2;
    uint8_t val, twos;
    int i;

    /*
     * Compute checksum and split the input into 3 pieces.
     */
    i = 0;
    chk0 = chk1 = chk2 = 0;
    while (true) {
        chk0 = (chk0 & 0xff) << 1;
        if (chk0 & 0x0100)
            chk0++;

        val = *sectorData++;
        chk2 += val;
        if (chk0 & 0x0100) {
            chk2++;
            chk0 &= 0xff;
        }
        part0[i] = (val ^ chk0) & 0xff;

        val = *sectorData++;
        chk1 += val;
        if (chk2 > 0xff) {
            chk1++;
            chk2 &= 0xff;
        }
        part1[i] = (val ^ chk2) & 0xff;

        if (sectorData - sectorDataStart == 524)
            break;

        val = *sectorData++;
        chk0 += val;
        if (chk1 > 0xff) {
            chk0++;
            chk1 &= 0xff;
        }
        part2[i] = (val ^ chk1) & 0xff;
        i++;
    }
    part2[kChunkSize35-1] = 0;  // gets merged into the "twos"

    assert(i == kChunkSize35-1);

    /*
     * Output the nibble data.
     */
    for (i = 0; i < kChunkSize35; i++) {
        twos =  ((part0[i] & 0xc0) >> 2) |
                ((part1[i] & 0xc0) >> 4) |
                ((part2[i] & 0xc0) >> 6);

        *outBuf++ = kDiskBytes62[twos];
        *outBuf++ = kDiskBytes62[part0[i] & 0x3f];
        *outBuf++ = kDiskBytes62[part1[i] & 0x3f];
        if (i != kChunkSize35 -1)
            *outBuf++ = kDiskBytes62[part2[i] & 0x3f];
    }

    /*
     * Output the checksum.
     */
    twos = ((chk0 & 0xc0) >> 6) | ((chk1 & 0xc0) >> 4) | ((chk2 & 0xc0) >> 2);
    *outBuf++ = kDiskBytes62[twos];
    *outBuf++ = kDiskBytes62[chk2 & 0x3f];
    *outBuf++ = kDiskBytes62[chk1 & 0x3f];
    *outBuf++ = kDiskBytes62[chk0 & 0x3f];

    assert(outBuf - outBufStart == kNibblizedOutputLen);
}
