/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Support for Formatted Disk Image (FDI) format.
 *
 * Based on the v2.0 spec and "fdi2raw.c".  The latter was released under
 * version 2 of the GPL, so this code may be subject to it.
 *
 * (Note: I tend to abuse the term "nibble" here.  Instead of 4 bits, I
 * use it to refer to 8 bits of "nibblized" data.  Sorry.)
 *
 * THOUGHT: we have access to the self-sync byte data.  We could use this
 * to pretty easily convert a track to 6656-byte format, which would allow
 * conversion to .NIB instead of .APP.  This would probably need to be
 * specified as a global preference (how to open .FDI), though we could
 * just drag the self-sync flags around in a parallel data structure and
 * invent a format-conversion API.  The former seems easier, and should
 * be easy to explain in the UI.
 */
#include "StdAfx.h"
#include "DiskImgPriv.h"


/*
 * ===========================================================================
 *      FDI compression functions
 * ===========================================================================
 */

/*
 * Pack a disk image with FDI.
 */
DIError WrapperFDI::PackDisk(GenericFD* pSrcGFD, GenericFD* pWrapperGFD)
{
    DIError dierr = kDIErrGeneric;      // not yet
    return dierr;
}


/*
 * ===========================================================================
 *      FDI expansion functions
 * ===========================================================================
 */

/*
 * Unpack an FDI-encoded disk image from "pGFD" to a new memory buffer
 * created in "*ppNewGFD".  The output is a collection of variable-length
 * nibble tracks.
 *
 * "pNewGFD" will need to hold (kTrackAllocSize * numCyls * numHeads)
 * bytes of data.
 *
 * Fills in "fNibbleTrackInfo".
 */
DIError WrapperFDI::UnpackDisk525(GenericFD* pGFD, GenericFD* pNewGFD,
    int numCyls, int numHeads)
{
    DIError dierr = kDIErrNone;
    uint8_t nibbleBuf[kNibbleBufLen];
    uint8_t* inputBuf = NULL;
    bool goodTracks[kMaxNibbleTracks525];
    int inputBufLen = -1;
    int badTracks = 0;
    int trk, type, length256;
    long nibbleLen;
    bool result;

    assert(numHeads == 1);
    memset(goodTracks, false, sizeof(goodTracks));

    dierr = pGFD->Seek(kMinHeaderLen, kSeekSet);
    if (dierr != kDIErrNone) {
        LOGI("FDI: track seek failed (offset=%d)", kMinHeaderLen);
        goto bail;
    }

    for (trk = 0; trk < numCyls * numHeads; trk++) {
        GetTrackInfo(trk, &type, &length256);
        LOGI("%2d.%d: t=0x%02x l=%d (%d)", trk / numHeads, trk % numHeads,
            type, length256, length256 * 256);

        /* if we have data to read, read it */
        if (length256 > 0) {
            if (length256  * 256 > inputBufLen) {
                /* allocate or increase the size of the input buffer */
                delete[] inputBuf;
                inputBufLen = length256 * 256;
                inputBuf = new uint8_t[inputBufLen];
                if (inputBuf == NULL) {
                    dierr = kDIErrMalloc;
                    goto bail;
                }
            }

            dierr = pGFD->Read(inputBuf, length256 * 256);
            if (dierr != kDIErrNone)
                goto bail;
        } else {
            assert(type == 0x00);
        }

        /* figure out what we want to do with this track */
        switch (type) {
        case 0x00:
            /* blank track */
            badTracks++;
            memset(nibbleBuf, 0xff, sizeof(nibbleBuf));
            nibbleLen = kTrackLenNb2525;
            break;
        case 0x80:
        case 0x90:
        case 0xa0:
        case 0xb0:
            /* low-level pulse-index */
            nibbleLen = kNibbleBufLen;
            result = DecodePulseTrack(inputBuf, length256*256, kBitRate525,
                        nibbleBuf, &nibbleLen);
            if (!result) {
                /* something failed in the decoder; fake it */
                badTracks++;
                memset(nibbleBuf, 0xff, sizeof(nibbleBuf));
                nibbleLen = kTrackLenNb2525;
            } else {
                goodTracks[trk] = true;
            }
            if (nibbleLen > kTrackAllocSize) {
                LOGI(" FDI: decoded %ld nibbles, buffer is only %d",
                    nibbleLen, kTrackAllocSize);
                dierr = kDIErrBadRawData;
                goto bail;
            }
            break;
        default:
            LOGI("FDI: unexpected track type 0x%04x", type);
            dierr = kDIErrUnsupportedImageFeature;
            goto bail;
        }

        fNibbleTrackInfo.offset[trk] = trk * kTrackAllocSize;
        fNibbleTrackInfo.length[trk] = nibbleLen;
        FixBadNibbles(nibbleBuf, nibbleLen);
        dierr = pNewGFD->Seek(fNibbleTrackInfo.offset[trk], kSeekSet);
        if (dierr != kDIErrNone)
            goto bail;
        dierr = pNewGFD->Write(nibbleBuf, nibbleLen);
        if (dierr != kDIErrNone)
            goto bail;
        LOGI("  FDI: track %d: wrote %ld nibbles", trk, nibbleLen);

        //offset += 256 * length256;
        //break;        // DEBUG DEBUG
    }

    LOGI(" FDI: %d of %d tracks bad or blank",
        badTracks, numCyls * numHeads);
    if (badTracks > (numCyls * numHeads) / 2) {
        LOGI("FDI: too many bad tracks");
        dierr = kDIErrBadRawData;
        goto bail;
    }

    /*
     * For convenience we want this to be 35 or 40 tracks.  Start by
     * reducing trk to 35 if there are no good tracks at 35+.
     */
    bool want40;
    int i;

    want40 = false;
    for (i = kTrackCount525; i < kMaxNibbleTracks525; i++) {
        if (goodTracks[i]) {
            want40 = true;
            break;
        }
    }
    if (!want40 && trk > kTrackCount525) {
        LOGI(" FDI: no good tracks past %d, reducing from %d",
            kTrackCount525, trk);
        trk = kTrackCount525;       // nothing good out there, roll back
    }

    /*
     * Now pad us *up* to 35 if we have fewer than that.
     */
    memset(nibbleBuf, 0xff, sizeof(nibbleBuf));
    for ( ; trk < kMaxNibbleTracks525; trk++) {
        if (trk == kTrackCount525)
            break;

        fNibbleTrackInfo.offset[trk] = trk * kTrackAllocSize;
        fNibbleTrackInfo.length[trk] = kTrackLenNb2525;
        fNibbleTrackInfo.numTracks++;

        dierr = pNewGFD->Seek(fNibbleTrackInfo.offset[trk], kSeekSet);
        if (dierr != kDIErrNone)
            goto bail;
        dierr = pNewGFD->Write(nibbleBuf, nibbleLen);
        if (dierr != kDIErrNone)
            goto bail;
    }

    assert(trk == kTrackCount525 || trk == kMaxNibbleTracks525);
    fNibbleTrackInfo.numTracks = trk;

bail:
    delete[] inputBuf;
    return dierr;
}

/*
 * Unpack an FDI-encoded disk image from "pGFD" to 800K of ProDOS-ordered
 * 512-byte blocks in "pNewGFD".
 *
 * We could keep the 12-byte "tags" on each block, but they were never
 * really used in the Apple II world.
 *
 * We also need to set up a "bad block" map to identify parts that we had
 * trouble unpacking.
 */
DIError WrapperFDI::UnpackDisk35(GenericFD* pGFD, GenericFD* pNewGFD, int numCyls,
    int numHeads, LinearBitmap* pBadBlockMap)
{
    DIError dierr = kDIErrNone;
    uint8_t nibbleBuf[kNibbleBufLen];
    uint8_t* inputBuf = NULL;
    uint8_t outputBuf[kMaxSectors35 * kBlockSize];    // 6KB
    int inputBufLen = -1;
    int badTracks = 0;
    int trk, type, length256;
    long nibbleLen;
    bool result;

    assert(numHeads == 2);

    dierr = pGFD->Seek(kMinHeaderLen, kSeekSet);
    if (dierr != kDIErrNone) {
        LOGI("FDI: track seek failed (offset=%d)", kMinHeaderLen);
        goto bail;
    }

    pNewGFD->Rewind();

    for (trk = 0; trk < numCyls * numHeads; trk++) {
        GetTrackInfo(trk, &type, &length256);
        LOGI("%2d.%d: t=0x%02x l=%d (%d)", trk / numHeads, trk % numHeads,
            type, length256, length256 * 256);

        /* if we have data to read, read it */
        if (length256 > 0) {
            if (length256  * 256 > inputBufLen) {
                /* allocate or increase the size of the input buffer */
                delete[] inputBuf;
                inputBufLen = length256 * 256;
                inputBuf = new uint8_t[inputBufLen];
                if (inputBuf == NULL) {
                    dierr = kDIErrMalloc;
                    goto bail;
                }
            }

            dierr = pGFD->Read(inputBuf, length256 * 256);
            if (dierr != kDIErrNone)
                goto bail;
        } else {
            assert(type == 0x00);
        }

        /* figure out what we want to do with this track */
        switch (type) {
        case 0x00:
            /* blank track */
            badTracks++;
            memset(nibbleBuf, 0xff, sizeof(nibbleBuf));
            nibbleLen = kTrackLenNb2525;
            break;
        case 0x80:
        case 0x90:
        case 0xa0:
        case 0xb0:
            /* low-level pulse-index */
            nibbleLen = kNibbleBufLen;
            result = DecodePulseTrack(inputBuf, length256*256,
                        BitRate35(trk/numHeads), nibbleBuf, &nibbleLen);
            if (!result) {
                /* something failed in the decoder; fake it */
                badTracks++;
                memset(nibbleBuf, 0xff, sizeof(nibbleBuf));
                nibbleLen = kTrackLenNb2525;
            } 
            if (nibbleLen > kNibbleBufLen) {
                LOGI(" FDI: decoded %ld nibbles, buffer is only %d",
                    nibbleLen, kTrackAllocSize);
                dierr = kDIErrBadRawData;
                goto bail;
            }
            break;
        default:
            LOGI("FDI: unexpected track type 0x%04x", type);
            dierr = kDIErrUnsupportedImageFeature;
            goto bail;
        }

        LOGI(" FDI: track %d got %ld nibbles", trk, nibbleLen);

        /*
        fNibbleTrackInfo.offset[trk] = trk * kTrackAllocSize;
        fNibbleTrackInfo.length[trk] = nibbleLen;
        dierr = pNewGFD->Seek(fNibbleTrackInfo.offset[trk], kSeekSet);
        if (dierr != kDIErrNone)
            goto bail;
        dierr = pNewGFD->Write(nibbleBuf, nibbleLen);
        if (dierr != kDIErrNone)
            goto bail;
        */

        dierr = DiskImg::UnpackNibbleTrack35(nibbleBuf, nibbleLen, outputBuf,
                    trk / numHeads, trk % numHeads, pBadBlockMap);
        if (dierr != kDIErrNone)
            goto bail;

        dierr = pNewGFD->Write(outputBuf,
                    kBlockSize * DiskImg::SectorsPerTrack35(trk / numHeads));
        if (dierr != kDIErrNone) {
            LOGI("FDI: failed writing disk blocks (%d * %d)",
                kBlockSize, DiskImg::SectorsPerTrack35(trk / numHeads));
            goto bail;
        }
    }

    //fNibbleTrackInfo.numTracks = numCyls * numHeads;

bail:
    delete[] inputBuf;
    return dierr;
}

/*
 * Return the approximate bit rate for the specified cylinder, in bits/sec.
 */
int WrapperFDI::BitRate35(int cyl)
{
    if (cyl >= 0 && cyl <= 15)
        return 375000;      // 394rpm
    else if (cyl <= 31)
        return 343750;      // 429rpm
    else if (cyl <= 47)
        return 312500;      // 472rpm
    else if (cyl <= 63)
        return 281250;      // 525rpm
    else if (cyl <= 79)
        return 250000;      // 590rpm
    else {
        LOGI(" FDI: invalid 3.5 cylinder %d", cyl);
        return 250000;
    }
}

/*
 * Fix any obviously-bad nibble values.
 *
 * This should be unlikely, but if we find several zeroes in a row due to
 * garbled data from the drive, it can happen.  We clean it up here so that,
 * when we convert to another format (e.g. TrackStar), we don't flunk a
 * simple high-bit screening test.
 *
 * (We could be more rigorous and test against valid disk bytes, but that's
 * probably excessive.)
 */
void WrapperFDI::FixBadNibbles(uint8_t* nibbleBuf, long nibbleLen)
{
    int badCount = 0;

    while (nibbleLen--) {
        if ((*nibbleBuf & 0x80) == 0) {
            badCount++;
            *nibbleBuf = 0xff;
        }
        nibbleBuf++;
    }

    if (badCount != 0) {
        LOGI("   FDI: fixed %d bad nibbles", badCount);
    }
}


/*
 * Get the info for the Nth track.  The track number is used as an index
 * into the track descriptor table.
 *
 * Returns the track type and amount of data (/256).
 */
void WrapperFDI::GetTrackInfo(int trk, int* pType, int* pLength256)
{
    uint16_t trackDescr;
    trackDescr = fHeaderBuf[kTrackDescrOffset + trk * 2] << 8 |
                 fHeaderBuf[kTrackDescrOffset + trk * 2 +1];

    *pType = (trackDescr & 0xff00) >> 8;
    *pLength256 = trackDescr & 0x00ff;

    switch (trackDescr & 0xf000) {
    case 0x0000:
        /* high-level type */
        switch (trackDescr & 0xff00) {
        case 0x0000:
            /* blank track */
            break;
        default:
            /* miscellaneous high-level type */
            break;
        }
        break;
    case 0x8000:
    case 0x9000:
    case 0xa000:
    case 0xb000:
        /* low-level type, length is 14 bits */
        *pType = (trackDescr & 0xc000) >> 8;
        *pLength256 = trackDescr & 0x3fff;
        break;
    case 0xc000:
    case 0xd000:
        /* mid-level format, value in 0n00 holds a bit rate index */
        break;
    case 0xe000:
    case 0xf000:
        /* raw MFM; for 0xf000, the value in 0n00 holds a bit rate index */
        break;
    default:
        LOGI("Unexpected trackDescr 0x%04x", trackDescr);
        *pType = 0x7e;      // return an invalid value
        *pLength256 = 0;
        break;
    }
}


/*
 * Convert a track encoded as one or more pulse streams to nibbles.
 *
 * This decompresses the pulse streams in "inputBuf", then converts them
 * to nibble form in "nibbleBuf".
 *
 * "*pNibbleLen" should hold the maximum size of the buffer.  On success,
 * it will hold the actual number of bytes used.
 *
 * Returns "true" on success, "false" on failure.
 */
bool WrapperFDI::DecodePulseTrack(const uint8_t* inputBuf, long inputLen,
    int bitRate, uint8_t* nibbleBuf, long* pNibbleLen)
{
    const int kSizeValueMask = 0x003fffff;
    const int kSizeCompressMask = 0x00c00000;
    const int kSizeCompressShift = 22;
    PulseIndexHeader hdr;
    uint32_t val;
    bool result = false;

    memset(&hdr, 0, sizeof(hdr));

    hdr.numPulses = GetLongBE(&inputBuf[0x00]);
    val = Get24BE(&inputBuf[0x04]);
    hdr.avgStreamLen = val & kSizeValueMask;
    hdr.avgStreamCompression = (val & kSizeCompressMask) >> kSizeCompressShift;
    val = Get24BE(&inputBuf[0x07]);
    hdr.minStreamLen = val & kSizeValueMask;
    hdr.minStreamCompression = (val & kSizeCompressMask) >> kSizeCompressShift;
    val = Get24BE(&inputBuf[0x0a]);
    hdr.maxStreamLen = val & kSizeValueMask;
    hdr.maxStreamCompression = (val & kSizeCompressMask) >> kSizeCompressShift;
    val = Get24BE(&inputBuf[0x0d]);
    hdr.idxStreamLen = val & kSizeValueMask;
    hdr.idxStreamCompression = (val & kSizeCompressMask) >> kSizeCompressShift;

    if (hdr.numPulses < 64 || hdr.numPulses > 131072) {
        /* should be about 40,000 */
        LOGI(" FDI: bad pulse count %ld in track", hdr.numPulses);
        return false;
    }

    /* advance past the 16 hdr bytes; now pointing at "average" stream */
    inputBuf += kPulseStreamDataOffset;

    LOGI("  pulses: %ld", hdr.numPulses);
    //LOGI("  avg: len=%d comp=%d", hdr.avgStreamLen, hdr.avgStreamCompression);
    //LOGI("  min: len=%d comp=%d", hdr.minStreamLen, hdr.minStreamCompression);
    //LOGI("  max: len=%d comp=%d", hdr.maxStreamLen, hdr.maxStreamCompression);
    //LOGI("  idx: len=%d comp=%d", hdr.idxStreamLen, hdr.idxStreamCompression);

    /*
     * Uncompress or endian-swap the pulse streams.
     */
    hdr.avgStream = new uint32_t[hdr.numPulses];
    if (hdr.avgStream == NULL)
        goto bail;
    if (!UncompressPulseStream(inputBuf, hdr.avgStreamLen, hdr.avgStream,
        hdr.numPulses, hdr.avgStreamCompression, 4))
    {
        goto bail;
    }
    inputBuf += hdr.avgStreamLen;

    if (hdr.minStreamLen > 0) {
        hdr.minStream = new uint32_t[hdr.numPulses];
        if (hdr.minStream == NULL)
            goto bail;
        if (!UncompressPulseStream(inputBuf, hdr.minStreamLen, hdr.minStream,
            hdr.numPulses, hdr.minStreamCompression, 4))
        {
            goto bail;
        }
        inputBuf += hdr.minStreamLen;
    }
    if (hdr.maxStreamLen > 0) {
        hdr.maxStream = new uint32_t[hdr.numPulses];
        if (!UncompressPulseStream(inputBuf, hdr.maxStreamLen, hdr.maxStream,
            hdr.numPulses, hdr.maxStreamCompression, 4))
        {
            goto bail;
        }
        inputBuf += hdr.maxStreamLen;
    }
    if (hdr.idxStreamLen > 0) {
        hdr.idxStream = new uint32_t[hdr.numPulses];
        if (!UncompressPulseStream(inputBuf, hdr.idxStreamLen, hdr.idxStream,
            hdr.numPulses, hdr.idxStreamCompression, 2))
        {
            goto bail;
        }
        inputBuf += hdr.idxStreamLen;
    }

    /*
     * Convert the pulse streams to a nibble stream.
     */
    result = ConvertPulseStreamsToNibbles(&hdr, bitRate, nibbleBuf, pNibbleLen);
    // fall through with result

bail:
    /* clean up */
    if (hdr.avgStream != NULL)
        delete[] hdr.avgStream;
    if (hdr.minStream != NULL)
        delete[] hdr.minStream;
    if (hdr.maxStream != NULL)
        delete[] hdr.maxStream;
    if (hdr.idxStream != NULL)
        delete[] hdr.idxStream;
    return result;
}

/*
 * Uncompress, or at least endian-swap, the input data.
 *
 * "inputLen" is the length in bytes of the input stream.  For an uncompressed
 * stream this should be equal to numPulses*bytesPerPulse, for a compressed
 * stream it's the length of the compressed data.
 *
 * "bytesPerPulse" indicates the width of the input data.  This will usually
 * be 4, but is 2 for the index stream.  The output is always 4 bytes/pulse.
 * For Huffman-compressed data, it appears that the input is always 4 bytes.
 *
 * Returns "true" if all went well, "false" if we hit something that we
 * couldn't handle.
 */
bool WrapperFDI::UncompressPulseStream(const uint8_t* inputBuf, long inputLen,
    uint32_t* outputBuf, long numPulses, int format, int bytesPerPulse)
{
    assert(bytesPerPulse == 2 || bytesPerPulse == 4);

    /*
     * Sample code has a snippet that says: if the format is "uncompressed"
     * but inputLen < (numPulses*2), treat it as compressed.  This may be
     * for handling some badly-formed images.  Not currently doing it here.
     */

    if (format == kCompUncompressed) {
        int i;

        LOGE("NOT TESTED");      // remove this when we've tested it

        if (inputLen != numPulses * bytesPerPulse) {
            LOGI(" FDI: got unc inputLen=%ld, outputLen=%ld",
                inputLen, numPulses * bytesPerPulse);
            return false;
        }
        if (bytesPerPulse == 2) {
            for (i = 0; i < numPulses; i++) {
                *outputBuf++ = GetShortBE(inputBuf);
                inputBuf += 2;
            }
        } else {
            for (i = 0; i < numPulses; i++) {
                *outputBuf++ = GetLongBE(inputBuf);
                inputBuf += 4;
            }
        }
    } else if (format == kCompHuffman) {
        if (!ExpandHuffman(inputBuf, inputLen, outputBuf, numPulses))
            return false;
        //LOGI("  FDI: Huffman expansion succeeded");
    } else {
        LOGI(" FDI: got weird compression format %d", format);
        return false;
    }

    return true;
}

/*
 * Expand a Huffman-compressed stream.
 *
 * The code takes bit-slices across the entire input and compresses them
 * separately with a static Huffman variant.
 *
 * "outputBuf" is expected to hold "numPulses" entries.
 *
 * This implementation is based on the fdi2raw code.
 */
bool WrapperFDI::ExpandHuffman(const uint8_t* inputBuf, long inputLen,
    uint32_t* outputBuf, long numPulses)
{
    HuffNode root;
    const uint8_t* origInputBuf = inputBuf;
    bool signExtend, sixteenBits;
    int i, subStreamShift;
    uint8_t bits;
    uint8_t bitMask;

    memset(outputBuf, 0, numPulses * sizeof(uint32_t));
    subStreamShift = 1;

    while (subStreamShift != 0) {
        if (inputBuf - origInputBuf >= inputLen) {
            LOGI("  FDI: overran input(1)");
            return false;
        }

        /* decode the sub-stream header */
        bits = *inputBuf++;
        subStreamShift = bits & 0x7f;           // low-order bit number
        signExtend = (bits & 0x80) != 0;
        bits = *inputBuf++;
        sixteenBits = (bits & 0x80) != 0;       // ignore redundant high-order

        //LOGI("   FDI: shift=%d ext=%d sixt=%d",
        //  subStreamShift, signExtend, sixteenBits);

        /* decode the Huffman tree structure */
        root.left = NULL;
        root.right = NULL;
        bitMask = 0;
        inputBuf = HuffExtractTree(inputBuf, &root, &bits, &bitMask);

        //LOGI("    after tree: off=%d", inputBuf - origInputBuf);

        /* extract the Huffman node values */
        if (sixteenBits)
            inputBuf = HuffExtractValues16(inputBuf, &root);
        else
            inputBuf = HuffExtractValues8(inputBuf, &root);

        if (inputBuf - origInputBuf >= inputLen) {
            LOGI("  FDI: overran input(2)");
            return false;
        }
        //LOGI("    after values: off=%d", inputBuf - origInputBuf);

        /* decode the data over all pulses */
        bitMask = 0;
        for (i = 0; i < numPulses; i++) {
            uint32_t outVal;
            const HuffNode* pCurrent = &root;

            /* chase down the tree until we hit a leaf */
            /* (note: nodes have two kids or none) */
            while (true) {
                if (pCurrent->left == NULL) {
                    break;
                } else {
                    bitMask >>= 1;
                    if (bitMask == 0) {
                        bitMask = 0x80;
                        bits = *inputBuf++;
                    }
                    if (bits & bitMask)
                        pCurrent = pCurrent->right;
                    else
                        pCurrent = pCurrent->left;
                }
            }

            outVal = outputBuf[i];
            if (signExtend) {
                if (sixteenBits)
                    outVal |= HuffSignExtend16(pCurrent->val) << subStreamShift;
                else
                    outVal |= HuffSignExtend8(pCurrent->val) << subStreamShift;
            } else {
                outVal |= pCurrent->val << subStreamShift;
            }
            outputBuf[i] = outVal;
        }
        HuffFreeNodes(root.left);
        HuffFreeNodes(root.right);
    }

    if (inputBuf - origInputBuf != inputLen) {
        LOGI("  FDI: warning: Huffman input %ld vs. %ld",
            inputBuf - origInputBuf, inputLen);
        return false;
    }

    return true;
}


/*
 * Recursively extract the Huffman tree structure for this sub-stream.
 */
const uint8_t* WrapperFDI::HuffExtractTree(const uint8_t* inputBuf,
    HuffNode* pNode, uint8_t* pBits, uint8_t* pBitMask)
{
    uint8_t val;

    if (*pBitMask == 0) {
        *pBits = *inputBuf++;
        *pBitMask = 0x80;
    }
    val = *pBits & *pBitMask;
    (*pBitMask) >>= 1;

    //LOGI("     val=%d", val);

    if (val != 0) {
        assert(pNode->left == NULL);
        assert(pNode->right == NULL);
        return inputBuf;
    } else {
        pNode->left = new HuffNode;
        memset(pNode->left, 0, sizeof(HuffNode));
        inputBuf = HuffExtractTree(inputBuf, pNode->left, pBits, pBitMask);
        pNode->right = new HuffNode;
        memset(pNode->right, 0, sizeof(HuffNode));
        return HuffExtractTree(inputBuf, pNode->right, pBits, pBitMask);
    }
}

/*
 * Recursively get the 16-bit values for our Huffman tree from the stream.
 */
const uint8_t* WrapperFDI::HuffExtractValues16(const uint8_t* inputBuf,
    HuffNode* pNode)
{
    if (pNode->left == NULL) {
        pNode->val = (*inputBuf++) << 8;
        pNode->val |= *inputBuf++;
        return inputBuf;
    } else {
        inputBuf = HuffExtractValues16(inputBuf, pNode->left);
        return HuffExtractValues16(inputBuf, pNode->right);
    }
}

/*
 * Recursively get the 8-bit values for our Huffman tree from the stream.
 */
const uint8_t* WrapperFDI::HuffExtractValues8(const uint8_t* inputBuf,
    HuffNode* pNode)
{
    if (pNode->left == NULL) {
        pNode->val = *inputBuf++;
        return inputBuf;
    } else {
        inputBuf = HuffExtractValues8(inputBuf, pNode->left);
        return HuffExtractValues8(inputBuf, pNode->right);
    }
}

/*
 * Recursively free up the current node and all nodes beneath it.
 */
void WrapperFDI::HuffFreeNodes(HuffNode* pNode)
{
    if (pNode != NULL) {
        HuffFreeNodes(pNode->left);
        HuffFreeNodes(pNode->right);
        delete pNode;
    }

}

/*
 * Sign-extend a 16-bit value to 32 bits.
 */
uint32_t WrapperFDI::HuffSignExtend16(uint32_t val)
{
    if (val & 0x8000)
        val |= 0xffff0000;
    return val;
}

/*
 * Sign-extend an 8-bit value to 32 bits.
 */
uint32_t WrapperFDI::HuffSignExtend8(uint32_t val)
{
    if (val & 0x80)
        val |= 0xffffff00;
    return val;
}


/* use these to extract values from the index stream */
#define ZeroStateCount(_val)    (((_val) >> 8) & 0xff)
#define OneStateCount(_val)     ((_val) & 0xff)

/*
 * Convert our collection of pulse streams into (what we hope will be)
 * Apple II nibble form.
 *
 * This modifies the contents of the minStream, maxStream, and idxStream
 * arrays.
 *
 * "*pNibbleLen" should hold the maximum size of the buffer.  On success,
 * it will hold the actual number of bytes used.
 */
bool WrapperFDI::ConvertPulseStreamsToNibbles(PulseIndexHeader* pHdr, int bitRate,
    uint8_t* nibbleBuf, long* pNibbleLen)
{
    uint32_t* fakeIdxStream = NULL;
    bool result = false;
    int i;

    /*
     * Stream pointers.  If we don't have a stream, fake it.
     */
    uint32_t* avgStream;
    uint32_t* minStream;
    uint32_t* maxStream;
    uint32_t* idxStream;
    
    avgStream = pHdr->avgStream;
    if (pHdr->minStream != NULL && pHdr->maxStream != NULL) {
        minStream = pHdr->minStream;
        maxStream = pHdr->maxStream;

        /* adjust the values in the min/max streams */
        for (i = 0; i < pHdr->numPulses; i++) {
            maxStream[i] = avgStream[i] + minStream[i] - maxStream[i];
            minStream[i] = avgStream[i] - minStream[i];
        }
    } else {
        minStream = pHdr->avgStream;
        maxStream = pHdr->avgStream;
    }

    if (pHdr->idxStream != NULL)
        idxStream = pHdr->idxStream;
    else {
        /*
         * The UAE sample code has some stuff to fake it.  The code there
         * is broken, so I'm guessing it has never been used, but I'm going
         * to replicate it here (and probably never test it either).  This
         * assumes that the original was written for a big-endian machine.
         */
        LOGI(" FDI: HEY: using fake index stream");
        DebugBreak();
        fakeIdxStream = new uint32_t[pHdr->numPulses];
        if (fakeIdxStream == NULL) {
            LOGI(" FDI: unable to alloc fake idx stream");
            goto bail;
        }
        for (i = 1; i < pHdr->numPulses; i++)
            fakeIdxStream[i] = 0x0200;      // '1' for two, '0' for zero
        fakeIdxStream[0] = 0x0101;          // '1' for one, '0' for one

        idxStream = fakeIdxStream;
    }

    /*
     * Compute a value for maxIndex.
     */
    uint32_t maxIndex;

    maxIndex = 0;
    for (i = 0; i < pHdr->numPulses; i++) {
        uint32_t sum;

        /* add up the two single-byte values in the index stream */
        sum = ZeroStateCount(idxStream[i]) + OneStateCount(idxStream[i]);
        if (sum > maxIndex)
            maxIndex = sum;
    }

    /*
     * Compute a value for indexOffset.
     */
    int indexOffset;

    indexOffset = 0;
    for (i = 0; i < pHdr->numPulses && OneStateCount(idxStream[i]) != 0; i++) {
        /* "falling edge, replace with ZeroStateCount for rising edge" */
    }
    if (i < pHdr->numPulses) {
        int start = i;
        do {
            i++;
            if (i >= pHdr->numPulses)
                i = 0;      // wrapped around
        } while (i != start && ZeroStateCount(idxStream[i]) == 0);
        if (i != start) {
            /* index pulse detected */
            while (i != start &&
                ZeroStateCount(idxStream[i]) > OneStateCount(idxStream[i]))
            {
                i++;
                if (i >= pHdr->numPulses)
                    i = 0;
            }
            if (i != start)
                indexOffset = i;    /* index position detected */
        }
    }

    /*
     * Compute totalAvg and weakBits, and rewrite idxStream.
     * (We don't actually use weakBits.)
     */
    uint32_t totalAvg;
    int weakBits;

    totalAvg = weakBits = 0;
    for (i = 0; i < pHdr->numPulses; i++) {
        unsigned int sum;
        sum = ZeroStateCount(idxStream[i]) + OneStateCount(idxStream[i]);
        if (sum >= maxIndex)
            totalAvg += avgStream[i];   // could this overflow...?
        else
            weakBits++;

        idxStream[i] = sum;
    }

    LOGI("     FDI: maxIndex=%u indexOffset=%d totalAvg=%d weakBits=%d",
        maxIndex, indexOffset, totalAvg, weakBits);

    /*
     * Take our altered stream values and the stuff we've calculated,
     * and convert the pulse values into bits.
     */
    uint8_t bitBuffer[kBitBufferSize];
    int bitCount;
    
    bitCount = kBitBufferSize;

    if (!ConvertPulsesToBits(avgStream, minStream, maxStream, idxStream,
        pHdr->numPulses, maxIndex, indexOffset, totalAvg, bitRate,
        bitBuffer, &bitCount))
    {
        LOGI(" FDI: ConvertPulsesToBits() failed");
        goto bail;
    }

    //LOGI("  Got %d bits", bitCount);
    if (bitCount < 0) {
        LOGI(" FDI: overran output bit buffer");
        goto bail;
    }

    /*
     * We have a bit stream with the GCR bits as they appear coming out of
     * the IWM.  Convert it to 8-bit nibble form.
     *
     * We currently discard self-sync byte information.
     */
    if (!ConvertBitsToNibbles(bitBuffer, bitCount, nibbleBuf, pNibbleLen))
    {
        LOGI(" FDI: ConvertBitsToNibbles() failed");
        goto bail;
    }

    result = true;

bail:
    delete[] fakeIdxStream;
    return result;
}


/*
 * Local data structures.  Not worth putting in the header file.
 */
const int kPulseLimitVal = 15;      /* "tolerance of 15%" */

typedef struct PulseSamples {
    uint32_t    size;
    int         numBits;
} PulseSamples;

class PulseSampleCollection {
public:
    PulseSampleCollection(void) {
        fArrayIndex = fTotalDiv = -1;
        fTotal = 0;
    }
    ~PulseSampleCollection(void) {}
    
    void Create(int stdMFM2BitCellSize, int numBits) {
        int i;

        fArrayIndex = 0;
        fTotal = 0;
        fTotalDiv = 0;
        for (i = 0; i < kSampleArrayMax; i++) {
            // "That is (total track length / 50000) for Amiga double density"
            fArray[i].size = stdMFM2BitCellSize;
            fTotal += fArray[i].size;
            fArray[i].numBits = numBits;
            fTotalDiv += fArray[i].numBits;
        }
        assert(fTotalDiv != 0);
    }

    uint32_t GetTotal(void) const { return fTotal; }
    int GetTotalDiv(void) const { return fTotalDiv; }

    void AdjustTotal(long val) { fTotal += val; }
    void AdjustTotalDiv(int val) { fTotalDiv += val; }
    void IncrIndex(void) {
        fArrayIndex++;
        if (fArrayIndex >= kSampleArrayMax)
            fArrayIndex = 0;
    }

    PulseSamples* GetCurrentArrayEntry(void) {
        return &fArray[fArrayIndex];
    }

    enum {
        kSampleArrayMax = 10,
    };

private:
    PulseSamples    fArray[kSampleArrayMax];
    int             fArrayIndex;
    uint32_t        fTotal;
    int             fTotalDiv;
};

#define MY_RANDOM
#ifdef MY_RANDOM
/* replace rand() with my function */
#define rand() MyRand()

/*
 * My psuedo-random number generator, which is even less random than
 * rand().  It is, however, consistent across all platforms, and the
 * value for RAND_MAX is small enough to avoid some integer overflow
 * problems that the code has with (2^31-1) implementations.
 */
#undef RAND_MAX
#define RAND_MAX    32767
int WrapperFDI::MyRand(void)
{
    const int kNumStates = 31;
    const int kQuantum = RAND_MAX / (kNumStates+1);
    static int state = 0;
    int retVal;

    state++;
    if (state == kNumStates)
        state = 0;

    retVal = (kQuantum * state) + (kQuantum / 2);
    assert(retVal >= 0 && retVal <= RAND_MAX);
    return retVal;
}
#endif

/*
 * Convert the pulses we've read to a bit stream.  This is a tad complex
 * because the FDI scanner was reading a GCR disk with an MFM drive.
 *
 * Pass the output buffer size in bytes in "*pOutputLen".  The actual number
 * of *bits* output is returned in it.
 *
 * This is a fairly direct conversion from the sample code.  There's a lot
 * here that I haven't taken the time to figure out.
 */
bool WrapperFDI::ConvertPulsesToBits(const uint32_t* avgStream,
    const uint32_t* minStream, const uint32_t* maxStream,
    const uint32_t* idxStream, int numPulses, int maxIndex,
    int indexOffset, uint32_t totalAvg, int bitRate,
    uint8_t* outputBuf, int* pOutputLen)
{
    PulseSampleCollection samples;
    BitOutputBuffer bitOutput(outputBuf, *pOutputLen);
    /* magic numbers, from somewhere */
    const uint32_t kStdMFM2BitCellSize = (totalAvg * 5) / bitRate;
    const uint32_t kStdMFM8BitCellSize = (totalAvg * 20) / bitRate;
    int mfmMagic = 0;       // if set to 1, decode as MFM rather than GCR
    bool result = false;
    int i;
    //int debugCounter = 0;

    /* sample code doesn't do this, but I want consistent results */
    srand(0);

    /*
     * "detects a long-enough stable pulse coming just after another
     * stable pulse"
     */
    i = 1;
    while (i < numPulses &&
           (idxStream[i] < (uint32_t) maxIndex ||
            idxStream[i-1] < (uint32_t) maxIndex ||
            minStream[i] < (kStdMFM2BitCellSize - (kStdMFM2BitCellSize / 4))
           ))
    {
        i++;
    }
    if (i == numPulses) {
        LOGW(" FDI: no stable and long-enough pulse in track");
        goto bail;
    }

    /*
     * Set up some variables.
     */
    int nextI, endOfData, adjust, /*bitOffset,*/ step;
    uint32_t refPulse;
    long jitter;

    samples.Create(kStdMFM2BitCellSize, 1 + mfmMagic);
    nextI = i;
    endOfData = i;
    i--;
    adjust = 0;
    //bitOffset = 0;
    refPulse = 0;
    jitter = 0;
    step = -1;

    /*
     * Run through the data three times:
     *  (-1) do stuff
     *  (0) do more stuff
     *  (1) output bits
     */
    while (step < 2) {
        /*
         * Calculates the current average bit rate from previously
         * decoded data.
         */
        uint32_t avgSize;
        int kCell8Limit = (kPulseLimitVal * kStdMFM8BitCellSize) / 100;

        /* this is the new average size for one MFM bit */
        avgSize = (samples.GetTotal() << (2 + mfmMagic)) / samples.GetTotalDiv();

        /*
         * Prevent avgSize from getting too far out of whack.
         *
         * "you can try tighter ranges than 25%, or wider ranges. I would
         * probably go for tighter..."
         */
        if ((avgSize < kStdMFM8BitCellSize - kCell8Limit) ||
            (avgSize > kStdMFM8BitCellSize + kCell8Limit))
        {
            avgSize = kStdMFM8BitCellSize;
        }

        /*
         * Get the next long-enough pulse (may require more than one pulse).
         */
        uint32_t pulse;

        pulse = 0;
        while (pulse < ((avgSize / 4) - (avgSize / 16))) {
            uint32_t avgPulse, minPulse, maxPulse;

            /* advance i */
            i++;
            if (i >= numPulses)
                i = 0;      // wrapped around

            /* advance nextI */
            if (i == nextI) {
                do {
                    nextI++;
                    if (nextI >= numPulses)
                        nextI = 0;
                } while (idxStream[nextI] < (uint32_t) maxIndex);
            }

            if (idxStream[i] >= (uint32_t) maxIndex) {
                /* stable pulse */
                avgPulse = avgStream[i] - jitter;
                minPulse = minStream[i];
                maxPulse = maxStream[i];
                if (jitter >= 0)
                    maxPulse -= jitter;
                else
                    minPulse -= jitter;

                if (maxStream[nextI] - avgStream[nextI] < avgPulse - minPulse)
                    minPulse = avgPulse - (maxStream[nextI] - avgStream[nextI]);
                if (avgStream[nextI] - minStream[nextI] < maxPulse - avgPulse)
                    maxPulse = avgPulse + (avgStream[nextI] - minStream[nextI]);
                if (minPulse < refPulse)
                    minPulse = refPulse;

                /*
                 * This appears to use a pseudo-random number generator
                 * to dither the signal.  This strikes me as highly
                 * questionable, but I'm trying to recreate what the sample
                 * code does, and I don't fully understand this stuff.
                 */
                int randVal;

                randVal = rand();
                if (randVal < (RAND_MAX / 2)) {
                    if (randVal > (RAND_MAX / 4)) {
                        if (randVal <= (3 * (RAND_MAX / 8)))
                            randVal = (2 * randVal) - (RAND_MAX / 4);
                        else
                            randVal = (4 * randVal) - RAND_MAX;
                    }
                    jitter = 0 - (randVal * (avgPulse - minPulse)) / RAND_MAX;
                } else {
                    randVal -= RAND_MAX / 2;
                    if (randVal > (RAND_MAX / 4)) {
                        if (randVal <= (3 * (RAND_MAX / 8)))
                            randVal = (2 * randVal) - (RAND_MAX / 4);
                        else
                            randVal = (4 * randVal) - RAND_MAX;
                    }
                    jitter = (randVal * (maxPulse - avgPulse)) / RAND_MAX;
                }
                avgPulse += jitter;

                if (avgPulse < minPulse || avgPulse > maxPulse) {
                    /* this is bad -- we're out of bounds */
                    LOGI("  FDI: avgPulse out of bounds: avg=%u min=%u max=%u",
                        avgPulse, minPulse, maxPulse);
                }
                if (avgPulse < refPulse) {
                    /* I guess this is also bad */
                    LOGI("  FDI: avgPulse < refPulse (%u %u)",
                        avgPulse, refPulse);
                }
                pulse += avgPulse - refPulse;
                refPulse = 0;

                /*
                 * If we've reached the end, advance to the next step.
                 */
                if (i == endOfData)
                    step++;
            } else if ((uint32_t) rand() <= (idxStream[i] * RAND_MAX) / maxIndex) {
                /* futz with it */
                int randVal;

                avgPulse = avgStream[i];
                minPulse = minStream[i];
                maxPulse = maxStream[i];

                randVal = rand();
                if (randVal < (RAND_MAX / 2)) {
                    if (randVal > (RAND_MAX / 4)) {
                        if (randVal <= (3 * (RAND_MAX / 8)))
                            randVal = (2 * randVal) - (RAND_MAX / 4);
                        else
                            randVal = (4 * randVal) - RAND_MAX;
                    }
                    avgPulse -= (randVal * (avgPulse - minPulse)) / RAND_MAX;
                } else {
                    randVal -= RAND_MAX / 2;
                    if (randVal > (RAND_MAX / 4)) {
                        if (randVal <= (3 * (RAND_MAX / 8)))
                            randVal = (2 * randVal) - (RAND_MAX / 4);
                        else
                            randVal = (4 * randVal) - RAND_MAX;
                    }
                    avgPulse += (randVal * (maxPulse - avgPulse)) / RAND_MAX;
                }
                if (avgPulse > refPulse &&
                    avgPulse < (avgStream[nextI] - jitter))
                {
                    pulse += avgPulse - refPulse;
                    refPulse = avgPulse;
                }
            } else {
                // do nothing
            }
        }

        /*
         * "gets the size in bits from the pulse width, considering the current
         * average bitrate"
         *
         * "realSize" will end up holding the number of bits we're going
         * to output for this pulse.
         */
        uint32_t adjustedPulse;
        int realSize;

        adjustedPulse = pulse;
        realSize = 0;
        if (mfmMagic != 0) {
            while (adjustedPulse >= avgSize) {
                realSize += 4;
                adjustedPulse -= avgSize / 2;
            }
            adjustedPulse <<= 3;
            while (adjustedPulse >= ((avgSize * 4) + (avgSize / 4))) {
                realSize += 2;
                adjustedPulse -= avgSize * 2;
            }
            if (adjustedPulse >= ((avgSize * 3) + (avgSize / 4))) {
                if (adjustedPulse <= ((avgSize * 4) - (avgSize / 4))) {
                    if ((2* ((adjustedPulse >> 2) - adjust)) <=
                        ((2 * avgSize) - (avgSize / 4)))
                    {
                        realSize += 3;
                    } else {
                        realSize += 4;
                    }
                } else {
                    realSize += 4;
                }
            } else {
                if (adjustedPulse > ((avgSize * 3) - (avgSize / 4))) {
                    realSize += 3;
                } else {
                    if (adjustedPulse >= ((avgSize * 2) + (avgSize / 4))) {
                        if ((2 * ((adjustedPulse >> 2) - adjust)) <
                            (avgSize + (avgSize / 4)))
                        {
                            realSize += 2;
                        } else {
                            realSize += 3;
                        }
                    } else {
                        realSize += 2;
                    }
                }
            }
        } else {
            /* mfmMagic == 0, whatever that means */
            while (adjustedPulse >= (2 * avgSize)) {
                realSize += 4;
                adjustedPulse -= avgSize;
            }
            adjustedPulse <<= 2;

            while (adjustedPulse >= ((avgSize * 3) + (avgSize / 4))) {
                realSize += 2;
                adjustedPulse -= avgSize * 2;
            }
            if (adjustedPulse >= ((avgSize * 2) + (avgSize / 4))) {
                if (adjustedPulse <= ((avgSize * 3) - (avgSize / 4))) {
                    if (((adjustedPulse >> 1) - adjust) <
                        (avgSize + (avgSize / 4)))
                    {
                        realSize += 2;
                    } else {
                        realSize += 3;
                    }
                } else {
                    realSize += 3;
                }
            } else {
                if (adjustedPulse > ((avgSize * 2) - (avgSize / 4)))
                    realSize += 2;
                else {
                    if (adjustedPulse >= (avgSize + (avgSize / 4))) {
                        if (((adjustedPulse >> 1) - adjust) <=
                            (avgSize - (avgSize / 4)))
                        {
                            realSize++;
                        } else {
                            realSize += 2;
                        }
                    } else {
                        realSize++;
                    }
                }
            }
        }

        /*
         * "after one pass to correctly initialize the average bitrate,
         * outputs the bits"
         */
        if (step == 1) {
            int j;

            for (j = realSize; j > 1; j--)
                bitOutput.WriteBit(0);
            bitOutput.WriteBit(1);
        }

        /*
         * Prepare for next pulse.
         */
        adjust = ((realSize * avgSize) / (4 << mfmMagic)) - pulse;

        PulseSamples* pSamples;
        pSamples = samples.GetCurrentArrayEntry();
        samples.AdjustTotal(-(long)pSamples->size);
        samples.AdjustTotalDiv(-pSamples->numBits);
        pSamples->size = pulse;
        pSamples->numBits = realSize;
        samples.AdjustTotal(pulse);
        samples.AdjustTotalDiv(realSize);
        samples.IncrIndex();
    }

    *pOutputLen = bitOutput.Finish();
    LOGI("     FDI: converted pulses to %d bits", *pOutputLen);
    result = true;

bail:
    return result;
}


/*
 * Convert a stream of GCR bits into nibbles.
 *
 * The stream includes 9-bit and 10-bit self-sync bytes.  We need to process
 * the bits as if we were an Apple II, shifting bits into a register until
 * we get a 1 in the msb.
 *
 * There is a (roughly) 7 in 8 chance that we will not start out reading
 * the stream on a byte boundary.  We have to read for a bit to let the
 * self-sync bytes do their job.
 *
 * "*pNibbleLen" should hold the maximum size of the buffer.  On success,
 * it will hold the actual number of bytes used.
 */
bool WrapperFDI::ConvertBitsToNibbles(const uint8_t* bitBuffer, int bitCount,
    uint8_t* nibbleBuf, long* pNibbleLen)
{
    BitInputBuffer inputBuffer(bitBuffer, bitCount);
    const uint8_t* nibbleBufStart = nibbleBuf;
    long outputBufSize = *pNibbleLen;
    bool result = false;
    uint8_t val;
    bool wrap;

    /*
     * Start 3/4 of the way through the buffer.  That should give us a
     * couple of self-sync zones before we hit the end of the buffer.
     */
    inputBuffer.SetStartPosition(3 * (bitCount / 4));

    /*
     * Run until we wrap.  We should be in sync by that point.
     */
    wrap = false;
    while (!wrap) {
        val = inputBuffer.GetByte(&wrap);
        if ((val & 0x80) == 0)
            val = (val << 1) | inputBuffer.GetBit(&wrap);
        if ((val & 0x80) == 0)
            val = (val << 1) | inputBuffer.GetBit(&wrap);
        if ((val & 0x80) == 0) {
            // not allowed by GCR encoding, probably garbage between sectors
            LOGI(" FDI: WARNING: more than 2 consecutive zeroes (sync)");
        }
    }

    /*
     * Extract the nibbles.
     */
    inputBuffer.ResetBitsConsumed();
    wrap = false;
    while (true) {
        val = inputBuffer.GetByte(&wrap);
        if ((val & 0x80) == 0)
            val = (val << 1) | inputBuffer.GetBit(&wrap);
        if ((val & 0x80) == 0)
            val = (val << 1) | inputBuffer.GetBit(&wrap);
        if ((val & 0x80) == 0) {
            LOGW(" FDI: WARNING: more than 2 consecutive zeroes (read)");
        }

        if (nibbleBuf - nibbleBufStart >= outputBufSize) {
            LOGW(" FDI: bits overflowed nibble buffer");
            goto bail;
        }
        *nibbleBuf++ = val;

        /* if we wrapped around on this one, we've reached the start point */
        if (wrap)
            break;
    }

    if (inputBuffer.GetBitsConsumed() != bitCount) {
        /* we dropped some or double-counted some */
        LOGW(" FDI: WARNING: consumed %d of %d bits",
            inputBuffer.GetBitsConsumed(), bitCount);
    }

    LOGI("   FDI: consumed %d of %d (first=0x%02x last=0x%02x)",
        inputBuffer.GetBitsConsumed(), bitCount,
        *nibbleBufStart, *(nibbleBuf-1));

    *pNibbleLen = nibbleBuf - nibbleBufStart;
    result = true;

bail:
    return result;
}
