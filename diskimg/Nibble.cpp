/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * DiskImg nibblized read/write functions.
 */
#include "StdAfx.h"
#include "DiskImgPriv.h"

/* define this for verbose output */
//#define NIB_VERBOSE_DEBUG


/*
 * ===========================================================================
 *      Nibble encoding and decoding
 * ===========================================================================
 */

/*static*/ uint8_t DiskImg::kDiskBytes53[32] = {
    0xab, 0xad, 0xae, 0xaf, 0xb5, 0xb6, 0xb7, 0xba,
    0xbb, 0xbd, 0xbe, 0xbf, 0xd6, 0xd7, 0xda, 0xdb,
    0xdd, 0xde, 0xdf, 0xea, 0xeb, 0xed, 0xee, 0xef,
    0xf5, 0xf6, 0xf7, 0xfa, 0xfb, 0xfd, 0xfe, 0xff
};
/*static*/ uint8_t DiskImg::kDiskBytes62[64] = {
    0x96, 0x97, 0x9a, 0x9b, 0x9d, 0x9e, 0x9f, 0xa6,
    0xa7, 0xab, 0xac, 0xad, 0xae, 0xaf, 0xb2, 0xb3,
    0xb4, 0xb5, 0xb6, 0xb7, 0xb9, 0xba, 0xbb, 0xbc,
    0xbd, 0xbe, 0xbf, 0xcb, 0xcd, 0xce, 0xcf, 0xd3,
    0xd6, 0xd7, 0xd9, 0xda, 0xdb, 0xdc, 0xdd, 0xde,
    0xdf, 0xe5, 0xe6, 0xe7, 0xe9, 0xea, 0xeb, 0xec,
    0xed, 0xee, 0xef, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6,
    0xf7, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff
};
/*static*/ uint8_t DiskImg::kInvDiskBytes53[256]; // all values are 0-31
/*static*/ uint8_t DiskImg::kInvDiskBytes62[256]; // all values are 0-63

/*
 * Compute tables to convert disk bytes back to values.
 *
 * Should be called once, at DLL initialization time.
 */
/*static*/ void DiskImg::CalcNibbleInvTables(void)
{
    unsigned int i;

    memset(kInvDiskBytes53, kInvInvalidValue, sizeof(kInvDiskBytes53));
    for (i = 0; i < sizeof(kDiskBytes53); i++) {
        assert(kDiskBytes53[i] >= 0x96);
        kInvDiskBytes53[kDiskBytes53[i]] = i;
    }

    memset(kInvDiskBytes62, kInvInvalidValue, sizeof(kInvDiskBytes62));
    for (i = 0; i < sizeof(kDiskBytes62); i++) {
        assert(kDiskBytes62[i] >= 0x96);
        kInvDiskBytes62[kDiskBytes62[i]] = i;
    }
}

/*
 * Find the start of the data field of a sector in nibblized data.
 *
 * Returns the index start on success or -1 on failure.
 */
int DiskImg::FindNibbleSectorStart(const CircularBufferAccess& buffer, int track,
    int sector, const NibbleDescr* pNibbleDescr, int* pVol)
{
    const int kMaxDataReach = 48;       // fairly arbitrary
    //DIError dierr;
    long trackLen = buffer.GetSize();

    assert(sector >= 0 && sector < 16);

    int i;

    for (i = 0; i < trackLen; i++) {
        bool foundAddr = false;

        if (pNibbleDescr->special == kNibbleSpecialSkipFirstAddrByte) {
            if (/*buffer[i] == pNibbleDescr->addrProlog[0] &&*/
                buffer[i+1] == pNibbleDescr->addrProlog[1] &&
                buffer[i+2] == pNibbleDescr->addrProlog[2])
            {
                foundAddr = true;
            }
        } else {
            if (buffer[i] == pNibbleDescr->addrProlog[0] &&
                buffer[i+1] == pNibbleDescr->addrProlog[1] &&
                buffer[i+2] == pNibbleDescr->addrProlog[2])
            {
                foundAddr = true;
            }
        }

        if (foundAddr) {
            //i += 3;

            /* found the address header, decode the address */
            short hdrVol, hdrTrack, hdrSector, hdrChksum;
            DecodeAddr(buffer, i+3, &hdrVol, &hdrTrack, &hdrSector,
                &hdrChksum);

            if (pNibbleDescr->addrVerifyTrack && track != hdrTrack) {
                LOGI("  Track mismatch (T=%d) got T=%d,S=%d",
                    track, hdrTrack, hdrSector);
                continue;
            }

            if (pNibbleDescr->addrVerifyChecksum) {
                if ((pNibbleDescr->addrChecksumSeed ^
                    hdrVol ^ hdrTrack ^ hdrSector ^ hdrChksum) != 0)
                {
                    LOGW("   Addr checksum mismatch (want T=%d,S=%d, got "
                          "T=%d,S=%d)",
                        track, sector, hdrTrack, hdrSector);
                    continue;
                }
            }

            i += 3;

            int j;
            for (j = 0; j < pNibbleDescr->addrEpilogVerifyCount; j++) {
                if (buffer[i+8+j] != pNibbleDescr->addrEpilog[j]) {
                    //LOGI("   Bad epilog byte %d (%02x vs %02x)",
                    //    j, buffer[i+8+j], pNibbleDescr->addrEpilog[j]);
                    break;
                }
            }
            if (j != pNibbleDescr->addrEpilogVerifyCount)
                continue;

#ifdef NIB_VERBOSE_DEBUG
            LOGI("    Good header, T=%d,S=%d (looking for T=%d,S=%d)",
                hdrTrack, hdrSector, track, sector);
#endif

            if (pNibbleDescr->special == kNibbleSpecialMuse) {
                /* e.g. original Castle Wolfenstein */
                if (track > 2) {
                    if ((hdrSector & 0x01) != 0)
                        continue;
                    hdrSector /= 2;
                }
            }

            if (sector != hdrSector)
                continue;

            /*
             * Scan forward and look for data prolog.  We want to limit
             * the reach of our search so we don't blunder into the data
             * field of the next sector.
             */
            for (j = 0; j < kMaxDataReach; j++) {
                if (buffer[i + j] == pNibbleDescr->dataProlog[0] &&
                    buffer[i + j +1] == pNibbleDescr->dataProlog[1] &&
                    buffer[i + j +2] == pNibbleDescr->dataProlog[2])
                {
                    *pVol = hdrVol;
                    return buffer.Normalize(i + j + 3);
                }
            }
        }
    }

#ifdef NIB_VERBOSE_DEBUG
    LOGI("   Couldn't find T=%d,S=%d", track, sector);
#endif
    return -1;
}

/*
 * Decode the values in the address field.
 */
void DiskImg::DecodeAddr(const CircularBufferAccess& buffer, int offset,
    short* pVol, short* pTrack, short* pSector, short* pChksum)
{
    //unsigned int vol, track, sector, chksum;

    *pVol = ConvFrom44(buffer[offset], buffer[offset+1]);
    *pTrack = ConvFrom44(buffer[offset+2], buffer[offset+3]);
    *pSector = ConvFrom44(buffer[offset+4], buffer[offset+5]);
    *pChksum = ConvFrom44(buffer[offset+6], buffer[offset+7]);
}

/*
 * Decode the sector pointed to by "pData" and described by "pNibbleDescr".
 * This invokes the appropriate function (e.g. 5&3 or 6&2) to decode the
 * data into a 256-byte sector.
 */
DIError DiskImg::DecodeNibbleData(const CircularBufferAccess& buffer, int idx,
    uint8_t* sctBuf, const NibbleDescr* pNibbleDescr)
{
    switch (pNibbleDescr->encoding) {
    case kNibbleEnc62:
        return DecodeNibble62(buffer, idx, sctBuf, pNibbleDescr);
    case kNibbleEnc53:
        return DecodeNibble53(buffer, idx, sctBuf, pNibbleDescr);
    default:
        assert(false);
        return kDIErrInternal;
    }
}

/*
 * Encode the sector pointed to by "pData" and described by "pNibbleDescr".
 * This invokes the appropriate function (e.g. 5&3 or 6&2) to encode the
 * data from a 256-byte sector.
 */
void DiskImg::EncodeNibbleData(const CircularBufferAccess& buffer, int idx,
    const uint8_t* sctBuf, const NibbleDescr* pNibbleDescr) const
{
    switch (pNibbleDescr->encoding) {
    case kNibbleEnc62:
        EncodeNibble62(buffer, idx, sctBuf, pNibbleDescr);
        break;
    case kNibbleEnc53:
        EncodeNibble53(buffer, idx, sctBuf, pNibbleDescr);
        break;
    default:
        assert(false);
        break;
    }
}

/*
 * Decode 6&2 encoding.
 */
DIError DiskImg::DecodeNibble62(const CircularBufferAccess& buffer, int idx,
    uint8_t* sctBuf, const NibbleDescr* pNibbleDescr)
{
    uint8_t twos[kChunkSize62 * 3];   // 258
    int chksum = pNibbleDescr->dataChecksumSeed;
    uint8_t decodedVal;
    int i;

    /*
     * Pull the 342 bytes out, convert them from disk bytes to 6-bit
     * values, and arrange them into a DOS-like pair of buffers.
     */
    for (i = 0; i < kChunkSize62; i++) {
        decodedVal = kInvDiskBytes62[buffer[idx++]];
        if (decodedVal == kInvInvalidValue)
            return kDIErrInvalidDiskByte;
        assert(decodedVal < sizeof(kDiskBytes62));

        chksum ^= decodedVal;
        twos[i] =
            ((chksum & 0x01) << 1) | ((chksum & 0x02) >> 1);
        twos[i + kChunkSize62] =
            ((chksum & 0x04) >> 1) | ((chksum & 0x08) >> 3);
        twos[i + kChunkSize62*2] =
            ((chksum & 0x10) >> 3) | ((chksum & 0x20) >> 5);
    }

    for (i = 0; i < 256; i++) {
        decodedVal = kInvDiskBytes62[buffer[idx++]];
        if (decodedVal == kInvInvalidValue)
            return kDIErrInvalidDiskByte;
        assert(decodedVal < sizeof(kDiskBytes62));

        chksum ^= decodedVal;
        sctBuf[i] = (chksum << 2) | twos[i];
    }

    /*
     * Grab the 343rd byte (the checksum byte) and see if we did this
     * right.
     */
    //printf("Dec checksum value is 0x%02x\n", chksum);
    decodedVal = kInvDiskBytes62[buffer[idx++]];
    if (decodedVal == kInvInvalidValue)
        return kDIErrInvalidDiskByte;
    assert(decodedVal < sizeof(kDiskBytes62));
    chksum ^= decodedVal;

    if (pNibbleDescr->dataVerifyChecksum && chksum != 0) {
        LOGI("    NIB bad data checksum");
        return kDIErrBadChecksum;
    }
    return kDIErrNone;
}

/*
 * Encode 6&2 encoding.
 */
void DiskImg::EncodeNibble62(const CircularBufferAccess& buffer, int idx,
    const uint8_t* sctBuf, const NibbleDescr* pNibbleDescr) const
{
    uint8_t top[256];
    uint8_t twos[kChunkSize62];
    int twoPosn, twoShift;
    int i;

    memset(twos, 0, sizeof(twos));

    twoShift = 0;
    for (i = 0, twoPosn = kChunkSize62-1; i < 256; i++) {
        unsigned int val = sctBuf[i];
        top[i] = val >> 2;
        twos[twoPosn] |= ((val & 0x01) << 1 | (val & 0x02) >> 1) << twoShift;

        if (twoPosn == 0) {
            twoPosn = kChunkSize62;
            twoShift += 2;
        }
        twoPosn--;
    }

    int chksum = pNibbleDescr->dataChecksumSeed;
    for (i = kChunkSize62-1; i >= 0; i--) {
        assert(twos[i] < sizeof(kDiskBytes62));
        buffer[idx++] = kDiskBytes62[twos[i] ^ chksum];
        chksum = twos[i];
    }

    for (i = 0; i < 256; i++) {
        assert(top[i] < sizeof(kDiskBytes62));
        buffer[idx++] = kDiskBytes62[top[i] ^ chksum];
        chksum = top[i];
    }

    //printf("Enc checksum value is 0x%02x\n", chksum);
    buffer[idx++] = kDiskBytes62[chksum];
}

/*
 * Decode 5&3 encoding.
 */
DIError DiskImg::DecodeNibble53(const CircularBufferAccess& buffer, int idx,
    uint8_t* sctBuf, const NibbleDescr* pNibbleDescr)
{
    uint8_t base[256];
    uint8_t threes[kThreeSize];
    int chksum = pNibbleDescr->dataChecksumSeed;
    uint8_t decodedVal;
    int i;

    /*
     * Pull the 410 bytes out, convert them from disk bytes to 5-bit
     * values, and arrange them into a DOS-like pair of buffers.
     */
    for (i = kThreeSize-1; i >= 0; i--) {
        decodedVal = kInvDiskBytes53[buffer[idx++]];
        if (decodedVal == kInvInvalidValue)
            return kDIErrInvalidDiskByte;
        assert(decodedVal < sizeof(kDiskBytes53));

        chksum ^= decodedVal;
        threes[i] = chksum;
    }

    for (i = 0; i < 256; i++) {
        decodedVal = kInvDiskBytes53[buffer[idx++]];
        if (decodedVal == kInvInvalidValue)
            return kDIErrInvalidDiskByte;
        assert(decodedVal < sizeof(kDiskBytes53));

        chksum ^= decodedVal;
        base[i] = (chksum << 3);
    }

    /*
     * Grab the 411th byte (the checksum byte) and see if we did this
     * right.
     */
    //printf("Dec checksum value is 0x%02x\n", chksum);
    decodedVal = kInvDiskBytes53[buffer[idx++]];
    if (decodedVal == kInvInvalidValue)
        return kDIErrInvalidDiskByte;
    assert(decodedVal < sizeof(kDiskBytes53));
    chksum ^= decodedVal;

    if (pNibbleDescr->dataVerifyChecksum && chksum != 0) {
        LOGI("    NIB bad data checksum (0x%02x)", chksum);
        return kDIErrBadChecksum;
    }

    /*
     * Convert this pile of stuff into 256 data bytes.
     */
    uint8_t* bufPtr;

    bufPtr = sctBuf;
    for (i = kChunkSize53-1; i >= 0; i--) {
        int three1, three2, three3, three4, three5;

        three1 = threes[i];
        three2 = threes[kChunkSize53 + i];
        three3 = threes[kChunkSize53*2 + i];
        three4 = (three1 & 0x02) << 1 | (three2 & 0x02) | (three3 & 0x02) >> 1;
        three5 = (three1 & 0x01) << 2 | (three2 & 0x01) << 1 | (three3 & 0x01);

        *bufPtr++ = base[i] | ((three1 >> 2) & 0x07);
        *bufPtr++ = base[kChunkSize53 + i] | ((three2 >> 2) & 0x07);
        *bufPtr++ = base[kChunkSize53*2 + i] | ((three3 >> 2) & 0x07);
        *bufPtr++ = base[kChunkSize53*3 + i] | (three4 & 0x07);
        *bufPtr++ = base[kChunkSize53*4 + i] | (three5 & 0x07);
    }
    assert(bufPtr == sctBuf + 255);

    /*
     * Convert the very last byte, which is handled specially.
     */
    *bufPtr = base[255] | (threes[kThreeSize-1] & 0x07);

    return kDIErrNone;
}

/*
 * Encode 5&3 encoding.
 */
void DiskImg::EncodeNibble53(const CircularBufferAccess& buffer, int idx,
    const uint8_t* sctBuf, const NibbleDescr* pNibbleDescr) const
{
    uint8_t top[kChunkSize53 * 5 +1];     // (255 / 0xff) +1
    uint8_t threes[kChunkSize53 * 3 +1];  // (153 / 0x99) +1
    int i, chunk;

    /*
     * Split the bytes into sections.
     */
    chunk = kChunkSize53-1;
    for (i = 0; i < (int) sizeof(top)-1; i += 5) {
        int three1, three2, three3, three4, three5;

        three1 = *sctBuf++;
        three2 = *sctBuf++;
        three3 = *sctBuf++;
        three4 = *sctBuf++;
        three5 = *sctBuf++;

        top[chunk] = three1 >> 3;
        top[chunk + kChunkSize53*1] = three2 >> 3;
        top[chunk + kChunkSize53*2] = three3 >> 3;
        top[chunk + kChunkSize53*3] = three4 >> 3;
        top[chunk + kChunkSize53*4] = three5 >> 3;

        threes[chunk] =
            (three1 & 0x07) << 2 | (three4 & 0x04) >> 1 | (three5 & 0x04) >> 2;
        threes[chunk + kChunkSize53*1] =
            (three2 & 0x07) << 2 | (three4 & 0x02) | (three5 & 0x02) >> 1;
        threes[chunk + kChunkSize53*2] =
            (three3 & 0x07) << 2 | (three4 & 0x01) << 1 | (three5 & 0x01);

        chunk--;
    }
    assert(chunk == -1);

    /*
     * Handle the last byte.
     */
    int val;
    val = *sctBuf++;
    top[255] = val >> 3;
    threes[kThreeSize-1] = val & 0x07;

    /*
     * Write the bytes.
     */
    int chksum = pNibbleDescr->dataChecksumSeed;
    for (i = sizeof(threes)-1; i >= 0; i--) {
        assert(threes[i] < sizeof(kDiskBytes53));
        buffer[idx++] = kDiskBytes53[threes[i] ^ chksum];
        chksum = threes[i];
    }

    for (i = 0; i < 256; i++) {
        assert(top[i] < sizeof(kDiskBytes53));
        buffer[idx++] = kDiskBytes53[top[i] ^ chksum];
        chksum = top[i];
    }

    //printf("Enc checksum value is 0x%02x\n", chksum);
    buffer[idx++] = kDiskBytes53[chksum];
}


/*
 * ===========================================================================
 *      Higher-level functions
 * ===========================================================================
 */

/*
 * Dump some bytes as hex values into a string.
 *
 * "buf" must be able to hold (num * 3) characters.
 */
static void DumpBytes(const uint8_t* bytes, unsigned int num, char* buf)
{
    sprintf(buf, "%02x", bytes[0]);
    buf += 2;

    for (int i = 1; i < (int) num; i++) {
        sprintf(buf, " %02x", bytes[i]);
        buf += 3;
    }

    *buf = '\0';
}

static inline const char* VerifyStr(bool val)
{
    return val ? "verify" : "ignore";
}

/*
 * Dump the contents of a NibbleDescr struct.
 */
void DiskImg::DumpNibbleDescr(const NibbleDescr* pNibDescr) const
{
    char outBuf1[48];
    char outBuf2[48];
    const char* encodingStr;

    switch (pNibDescr->encoding) {
    case kNibbleEnc62:  encodingStr = "6&2";    break;
    case kNibbleEnc53:  encodingStr = "5&3";    break;
    case kNibbleEnc44:  encodingStr = "4&4";    break;
    default:            encodingStr = "???";    break;
    }

    LOGI("NibbleDescr '%s':", pNibDescr->description);
    LOGI("  Nibble encoding is %s", encodingStr);
    DumpBytes(pNibDescr->addrProlog, sizeof(pNibDescr->addrProlog), outBuf1);
    DumpBytes(pNibDescr->dataProlog, sizeof(pNibDescr->dataProlog), outBuf2);
    LOGI("  Addr prolog: %s          Data prolog: %s", outBuf1, outBuf2);
    DumpBytes(pNibDescr->addrEpilog, sizeof(pNibDescr->addrEpilog), outBuf1);
    DumpBytes(pNibDescr->dataEpilog, sizeof(pNibDescr->dataEpilog), outBuf2);
    LOGI("  Addr epilog: %s (%d)      Data epilog: %s (%d)",
        outBuf1, pNibDescr->addrEpilogVerifyCount,
        outBuf2, pNibDescr->dataEpilogVerifyCount);
    LOGI("  Addr checksum: %s          Data checksum: %s",
        VerifyStr(pNibDescr->addrVerifyChecksum),
        VerifyStr(pNibDescr->dataVerifyChecksum));
    LOGI("  Addr checksum seed: 0x%02x       Data checksum seed: 0x%02x",
        pNibDescr->addrChecksumSeed, pNibDescr->dataChecksumSeed);
    LOGI("  Addr check track: %s",
        VerifyStr(pNibDescr->addrVerifyTrack));
}


/*
 * Load a nibble track into our track buffer.
 */
DIError DiskImg::LoadNibbleTrack(long track, long* pTrackLen)
{
    DIError dierr = kDIErrNone;
    long offset;
    assert(track >= 0 && track < kMaxNibbleTracks525);

    *pTrackLen = GetNibbleTrackLength(track);
    offset = GetNibbleTrackOffset(track);
    assert(*pTrackLen > 0);
    assert(offset >= 0);

    if (track == fNibbleTrackLoaded) {
#ifdef NIB_VERBOSE_DEBUG
        LOGI("  DI track %d already loaded", track);
#endif
        return kDIErrNone;
    } else {
        LOGI("  DI loading track %ld", track);
    }

    /* invalidate in case we fail with partial read */
    fNibbleTrackLoaded = -1;

    /* alloc track buffer if needed */
    if (fNibbleTrackBuf == NULL) {
        fNibbleTrackBuf = new uint8_t[kTrackAllocSize];
        if (fNibbleTrackBuf == NULL)
            return kDIErrMalloc;
    }

    /*
     * Read the entire track into memory.
     */
    dierr = CopyBytesOut(fNibbleTrackBuf, offset, *pTrackLen);
    if (dierr != kDIErrNone)
        return dierr;

    fNibbleTrackLoaded = track;

    return dierr;
}

/*
 * Save the track buffer back to disk.
 */
DIError DiskImg::SaveNibbleTrack(void)
{
    if (fNibbleTrackLoaded < 0) {
        LOGI("ERROR: tried to save track without loading it first");
        return kDIErrInternal;
    }
    assert(fNibbleTrackBuf != NULL);

    DIError dierr = kDIErrNone;
    long trackLen = GetNibbleTrackLength(fNibbleTrackLoaded);
    long offset = GetNibbleTrackOffset(fNibbleTrackLoaded);

    /* write the track to fpDataGFD */
    dierr = CopyBytesIn(fNibbleTrackBuf, offset, trackLen);
    return dierr;
}


/*
 * Count up the number of readable sectors found on this track, and
 * return it.  If "pVol" is non-NULL, return the volume number from
 * one of the readable sectors.
 */
int DiskImg::TestNibbleTrack(int track, const NibbleDescr* pNibbleDescr,
    int* pVol)
{
    long trackLen;
    int count = 0;

    assert(track >= 0 && track < kTrackCount525);
    assert(pNibbleDescr != NULL);

    if (LoadNibbleTrack(track, &trackLen) != kDIErrNone) {
        LOGI("   DI FindNibbleSectorStart: LoadNibbleTrack failed");
        return 0;
    }

    CircularBufferAccess buffer(fNibbleTrackBuf, trackLen);

    int i, sectorIdx;
    for (i = 0; i < pNibbleDescr->numSectors; i++) {
        int vol;
        sectorIdx = FindNibbleSectorStart(buffer, track, i, pNibbleDescr, &vol);
        if (sectorIdx >= 0) {
            if (pVol != NULL)
                *pVol = vol;

            uint8_t sctBuf[256];
            if (DecodeNibbleData(buffer, sectorIdx, sctBuf, pNibbleDescr) == kDIErrNone)
                count++;
        }
    }

    LOGI("   Tests on track=%d with '%s' returning count=%d",
        track, pNibbleDescr->description, count);

    return count;
}

/*
 * Analyze the nibblized track data.
 *
 * On entry:
 *  fPhysical indicates the appropriate nibble format
 *
 * On exit:
 *  fpNibbleDescr points to the most-likely-to-succeed NibbleDescr
 *  fDOSVolumeNum holds a volume number from one of the tracks
 *  fNumTracks holds the number of tracks on the disk
 */
DIError DiskImg::AnalyzeNibbleData(void)
{
    assert(IsNibbleFormat(fPhysical));

    if (fPhysical == kPhysicalFormatNib525_Var) {
        /* TrackStar can have up to 40 */
        fNumTracks = fpImageWrapper->GetNibbleNumTracks();
        assert(fNumTracks > 0);
    } else {
        /* fixed-length formats (.nib, .nb2) are always 35 tracks */
        fNumTracks = kTrackCount525;
    }

    /*
     * Try to read sectors from tracks 1, 16, 17, and 26.  If we can get
     * at least 13 out of 16 (or 10 out of 13) on three out of four tracks,
     * we have a winner.
     */
    int i, good, goodTracks;
    int protoVol = kVolumeNumNotSet;

    for (i = 0; i < fNumNibbleDescrEntries; i++) {
        if (fpNibbleDescrTable[i].numSectors == 0) {
            /* uninitialized "custom" entry */
            LOGI("  Skipping '%s'", fpNibbleDescrTable[i].description);
            continue;
        }
        LOGI("  Trying '%s'", fpNibbleDescrTable[i].description);
        goodTracks = 0;

        good = TestNibbleTrack(1, &fpNibbleDescrTable[i], NULL);
        if (good > fpNibbleDescrTable[i].numSectors - 4)
            goodTracks++;
        good = TestNibbleTrack(16, &fpNibbleDescrTable[i], NULL);
        if (good > fpNibbleDescrTable[i].numSectors - 4)
            goodTracks++;
        good = TestNibbleTrack(17, &fpNibbleDescrTable[i], &protoVol);
        if (good > fpNibbleDescrTable[i].numSectors - 4)
            goodTracks++;
        good = TestNibbleTrack(26, &fpNibbleDescrTable[i], NULL);
        if (good > fpNibbleDescrTable[i].numSectors - 4)
            goodTracks++;

        if (goodTracks >= 3) {
            LOGI("  Looks like '%s' (%d-sector), vol=%d",
                fpNibbleDescrTable[i].description,
                fpNibbleDescrTable[i].numSectors, protoVol);
            fpNibbleDescr = &fpNibbleDescrTable[i];
            fDOSVolumeNum = protoVol;
            break;
        }
    }
    if (i == fNumNibbleDescrEntries) {
        LOGI("AnalyzeNibbleData did not find matching NibbleDescr");
        return kDIErrBadNibbleSectors;
    }

    return kDIErrNone;
}

/*
 * Read a sector from a nibble image.
 *
 * While fNumTracks is valid, fNumSectPerTrack is a little flaky, because
 * in theory each track could be formatted differently.
 */
DIError DiskImg::ReadNibbleSector(long track, int sector, void* buf,
    const NibbleDescr* pNibbleDescr)
{
    if (pNibbleDescr == NULL) {
        /* disk has no recognizable sectors */
        LOGI(" DI ReadNibbleSector: pNibbleDescr is NULL, returning failure");
        return kDIErrBadNibbleSectors;
    }
    if (sector >= pNibbleDescr->numSectors) {
        /* e.g. trying to read sector 14 on a 13-sector disk */
        LOGI(" DI ReadNibbleSector: bad sector number request");
        return kDIErrInvalidSector;
    }

    assert(pNibbleDescr != NULL);
    assert(IsNibbleFormat(fPhysical));
    assert(track >= 0 && track < GetNumTracks());
    assert(sector >= 0 && sector < pNibbleDescr->numSectors);

    DIError dierr = kDIErrNone;
    long trackLen;
    int sectorIdx, vol;

    dierr = LoadNibbleTrack(track, &trackLen);
    if (dierr != kDIErrNone) {
        LOGI("   DI ReadNibbleSector: LoadNibbleTrack %ld failed", track);
        return dierr;
    }

    CircularBufferAccess buffer(fNibbleTrackBuf, trackLen);
    sectorIdx = FindNibbleSectorStart(buffer, track, sector, pNibbleDescr,
                    &vol);
    if (sectorIdx < 0)
        return kDIErrSectorUnreadable;

    dierr = DecodeNibbleData(buffer, sectorIdx, (uint8_t*) buf,
                pNibbleDescr);

    return dierr;
}

/*
 * Write a sector to a nibble image.
 */
DIError DiskImg::WriteNibbleSector(long track, int sector, const void* buf,
    const NibbleDescr* pNibbleDescr)
{
    assert(pNibbleDescr != NULL);
    assert(IsNibbleFormat(fPhysical));
    assert(track >= 0 && track < GetNumTracks());
    assert(sector >= 0 && sector < pNibbleDescr->numSectors);
    assert(!fReadOnly);

    DIError dierr = kDIErrNone;
    long trackLen;
    int sectorIdx, vol;

    dierr = LoadNibbleTrack(track, &trackLen);
    if (dierr != kDIErrNone) {
        LOGI("   DI ReadNibbleSector: LoadNibbleTrack %ld failed", track);
        return dierr;
    }

    CircularBufferAccess buffer(fNibbleTrackBuf, trackLen);
    sectorIdx = FindNibbleSectorStart(buffer, track, sector, pNibbleDescr,
                    &vol);
    if (sectorIdx < 0)
        return kDIErrSectorUnreadable;

    EncodeNibbleData(buffer, sectorIdx, (uint8_t*) buf, pNibbleDescr);

    dierr = SaveNibbleTrack();
    if (dierr != kDIErrNone) {
        LOGI("   DI ReadNibbleSector: SaveNibbleTrack %ld failed", track);
        return dierr;
    }

    return dierr;
}

/*
 * Get the contents of the nibble track.
 *
 * "buf" must be able to hold kTrackAllocSize bytes.
 */
DIError DiskImg::ReadNibbleTrack(long track, uint8_t* buf, long* pTrackLen)
{
    DIError dierr;

    dierr = LoadNibbleTrack(track, pTrackLen);
    if (dierr != kDIErrNone) {
        LOGI("   DI ReadNibbleTrack: LoadNibbleTrack %ld failed", track);
        return dierr;
    }

    memcpy(buf, fNibbleTrackBuf, *pTrackLen);
    return kDIErrNone;
}

/*
 * Set the contents of a nibble track.
 *
 * NOTE: This currently does the wrong thing when converting from .nb2 to
 * .nib.  Fixed-length formats shouldn't be allowed to interact.  Figure
 * this out someday.  For now, the higher-level code prevents it.
 */
DIError DiskImg::WriteNibbleTrack(long track, const uint8_t* buf, long trackLen)
{
    DIError dierr;
    long oldTrackLen;

    /* load the track to set the "current track" stuff */
    dierr = LoadNibbleTrack(track, &oldTrackLen);
    if (dierr != kDIErrNone) {
        LOGI("   DI WriteNibbleTrack: LoadNibbleTrack %ld failed", track);
        return dierr;
    }

    if (trackLen > GetNibbleTrackAllocLength()) {
        LOGI("ERROR: tried to write too-long track len (%ld vs %d)",
            trackLen, GetNibbleTrackAllocLength());
        return kDIErrInvalidArg;
    }

    if (trackLen < oldTrackLen)     // pad out any extra space
        memset(fNibbleTrackBuf, 0xff, oldTrackLen);
    memcpy(fNibbleTrackBuf, buf, trackLen);
    fpImageWrapper->SetNibbleTrackLength(track, trackLen);

    dierr = SaveNibbleTrack();
    if (dierr != kDIErrNone) {
        LOGI("   DI ReadNibbleSector: SaveNibbleTrack %ld failed", track);
        return dierr;
    }

    return kDIErrNone;
}

/*
 * Create a blank nibble image, using fpNibbleDescr as the template.
 * Sets "fLength".
 *
 * Tracks are written the same way regardless of actual track length (be
 * it 6656, 6384, or variable-length).  Anything longer than 6384 just has
 * more padding at the end of the track.
 *
 * The format looks like this:
 *  Gap one (48 self-sync bytes)
 *  For each sector:
 *   Address field (14 bytes, e.g. d5aa96 vol track sect chksum deaaeb)
 *   Gap two (six self-sync bytes)
 *   Data field (6 header bytes, 1 checksum byte, and 342 or 410 data bytes)
 *   Gap three (27 self-sync bytes)
 *
 * 48 + (14 + 6 + (6 + 1 + 342) + 27) * 16 = 6384
 * 48 + (14 + 6 + (6 + 1 + 410) + 27) * 13 = 6080
 */
DIError DiskImg::FormatNibbles(GenericFD* pGFD) const
{
    assert(fHasNibbles);
    assert(GetNumTracks() > 0);

    DIError dierr = kDIErrNone;
    uint8_t trackBuf[kTrackAllocSize];
    /* these should be the same except for var-len images */
    long trackAllocLen = GetNibbleTrackAllocLength();
    long trackLen = GetNibbleTrackFormatLength();
    int track;

    assert(trackLen > 0);
    pGFD->Rewind();

    /*
     * If we don't have sector access, take a shortcut and just fill the
     * entire image with 0xff.
     */
    if (!fHasSectors) {
        memset(trackBuf, 0xff, trackLen);
        for (track = 0; track < GetNumTracks(); track++) {
            /* write the track to the GFD */
            dierr = pGFD->Write(trackBuf, trackAllocLen);
            if (dierr != kDIErrNone)
                return dierr;
            fpImageWrapper->SetNibbleTrackLength(track, trackAllocLen);
        }

        return kDIErrNone;
    }


    assert(fHasSectors);
    assert(fpNibbleDescr != NULL);
    assert(fpNibbleDescr->numSectors == GetNumSectPerTrack());
    assert(fpNibbleDescr->encoding == kNibbleEnc53 ||
           fpNibbleDescr->encoding == kNibbleEnc62);
    assert(fDOSVolumeNum != kVolumeNumNotSet);

    /*
     * Create a prototype sector.  The data for a sector full of zeroes
     * is exactly the same; only the address header changes.
     */
    uint8_t sampleSource[256];
    uint8_t sampleBuf[512];      // must hold 5&3 and 6&2
    CircularBufferAccess sample(sampleBuf, 512);
    long dataLen;

    if (fpNibbleDescr->encoding == kNibbleEnc53)
        dataLen = 410 +1;
    else
        dataLen = 342 +1;

    memset(sampleSource, 0, sizeof(sampleSource));
    EncodeNibbleData(sample, 0, sampleSource, fpNibbleDescr);

    /*
     * For each track in the image, "format" the expected number of
     * sectors, then write the data to the GFD.
     */
    for (track = 0; track < GetNumTracks(); track++) {
        //LOGI("Formatting track %d", track);
        uint8_t* trackPtr = trackBuf;

        /*
         * Fill with "self-sync" bytes.
         */
        memset(trackBuf, 0xff, trackAllocLen);

        /* gap one */
        trackPtr += 48;

        for (int sector = 0; sector < fpNibbleDescr->numSectors; sector++) {
            /*
             * Write address field.
             */
            uint16_t hdrTrack, hdrSector, hdrVol, hdrChksum;
            hdrTrack = track;
            hdrSector = sector;
            hdrVol = fDOSVolumeNum;
            *trackPtr++ = fpNibbleDescr->addrProlog[0];
            *trackPtr++ = fpNibbleDescr->addrProlog[1];
            *trackPtr++ = fpNibbleDescr->addrProlog[2];
            *trackPtr++ = Conv44(hdrVol, true);
            *trackPtr++ = Conv44(hdrVol, false);
            *trackPtr++ = Conv44(hdrTrack, true);
            *trackPtr++ = Conv44(hdrTrack, false);
            *trackPtr++ = Conv44(hdrSector, true);
            *trackPtr++ = Conv44(hdrSector, false);
            hdrChksum = fpNibbleDescr->addrChecksumSeed ^
                            hdrVol ^ hdrTrack ^ hdrSector;
            *trackPtr++ = Conv44(hdrChksum, true);
            *trackPtr++ = Conv44(hdrChksum, false);
            *trackPtr++ = fpNibbleDescr->addrEpilog[0];
            *trackPtr++ = fpNibbleDescr->addrEpilog[1];
            *trackPtr++ = fpNibbleDescr->addrEpilog[2];

            /* gap two */
            trackPtr += 6;

            /*
             * Write data field.
             */
            *trackPtr++ = fpNibbleDescr->dataProlog[0];
            *trackPtr++ = fpNibbleDescr->dataProlog[1];
            *trackPtr++ = fpNibbleDescr->dataProlog[2];
            memcpy(trackPtr, sampleBuf, dataLen);
            trackPtr += dataLen;
            *trackPtr++ = fpNibbleDescr->dataEpilog[0];
            *trackPtr++ = fpNibbleDescr->dataEpilog[1];
            *trackPtr++ = fpNibbleDescr->dataEpilog[2];

            /* gap three */
            trackPtr += 27;
        }

        assert(trackPtr - trackBuf == 6384 ||
               trackPtr - trackBuf == 6080);

        /*
         * Write the track to the GFD.
         */
        dierr = pGFD->Write(trackBuf, trackAllocLen);
        if (dierr != kDIErrNone)
            break;

        /* on a variable-length image, reduce track len to match */
        fpImageWrapper->SetNibbleTrackLength(track, trackLen);
    }

    return dierr;
}
