/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Convert MacPaint 'PNTG' images.
 */
#include "StdAfx.h"
#include "MacPaint.h"

/*
 * The files are always 576x720 monochrome.
 *
 * This format often starts with a MacBinary header, especially for files
 * posted to BBSs.  The FileType should be 'PNTG', and the CreatorType will
 * usually be 'MPNT'.
 *
 * Following the 128-byte header (or at the very start of the data fork for
 * a file on the Mac) is:
 *
 *  +000-003: 4-byte big-endian version number (0 or 2)
 *  +004-307: pattern data (38 patterns * 8 bytes), not needed to decode image
 *  +308-511: 240 bytes of zeroes, used for padding
 *  +512-nnn: PackBits-encoded scan lines (72 bytes of output each)
 *
 * The files seem to be stored in 512-byte chunks, but it's probably unwise to
 * expect anything other than some extra bytes at the end.
 *
 * The bits are in the same order as Windows BMP, but white/black are reversed
 * (i.e. a '1' bit indicates a black pixel).  This can be accommodated by
 * changing the palette colors or by inverting the bits.
 *
 * Corrupted MacPaint files seem to be popular.  Handling corruption
 * gracefully is important.
 */

/*
 * If the file ends in ".mac", we accept it if it has a MacBinary header or
 * if it begins with 00000002 and exceeds the minimum size required.
 */
void ReformatMacPaint::Examine(ReformatHolder* pHolder)
{
    ReformatHolder::ReformatApplies applies = ReformatHolder::kApplicNot;
    const uint8_t* ptr = pHolder->GetSourceBuf(ReformatHolder::kPartData);
    long fileLen = pHolder->GetSourceLen(ReformatHolder::kPartData);
    const char* nameExt = pHolder->GetNameExt();
    long version;

    if (fileLen < kMinSize) {
        LOGI("  MP: not long enough to be MacPaint (%d vs min %d)",
            fileLen, kMinSize);
        goto done;
    } 

    version = Get32BE(ptr);
    if (stricmp(nameExt, ".mac") == 0 && version >= 0 && version <= 3) {
        LOGI("  MP: found w/o MacBinary header");
        applies = ReformatHolder::kApplicProbably;
        goto done;
    }

    version = Get32BE(ptr + 128);
    if (version >= 0 && version <= 3 &&
        ptr[65] == 'P' && ptr[66] == 'N' && ptr[67] == 'T' && ptr[68] == 'G')
    {
        LOGI("  MP: found inside MacBinary header");
        applies = ReformatHolder::kApplicProbably;
        goto done;
    }

done:
    pHolder->SetApplic(ReformatHolder::kReformatMacPaint, applies,
        ReformatHolder::kApplicNot, ReformatHolder::kApplicNot);
}

/*
 * Convert the image to a monochrome bitmap.
 */
int ReformatMacPaint::Process(const ReformatHolder* pHolder,
    ReformatHolder::ReformatID id, ReformatHolder::ReformatPart part,
    ReformatOutput* pOutput)
{
    MyDIBitmap* pDib;
    const uint8_t* srcBuf = pHolder->GetSourceBuf(part);
    long srcLen = pHolder->GetSourceLen(part);
    int retval = -1;

    if (srcLen < kMinSize || srcLen > kMaxSize) {
        LOGI(" MacPaint file is only %d bytes long", srcLen);
        goto bail;
    }

    pDib = ConvertMacPaint(srcBuf, srcLen);
    if (pDib == NULL)
        goto bail;

    SetResultBuffer(pOutput, pDib);
    retval = 0;

bail:
    return retval;
}

/*
 * Handle the actual conversion.
 *
 * 576 pixels per line == 72 bytes per line, which is a multiple of 4 bytes
 * (required for windows BMP).
 */
MyDIBitmap* ReformatMacPaint::ConvertMacPaint(const uint8_t* srcBuf, long length)
{
    MyDIBitmap* pDib = NULL;
    uint8_t* outBuf;
    static const RGBQUAD colorConv[2] = {
        /* blue, green, red, reserved */
        { 0x00, 0x00, 0x00 },   // black
        { 0xff, 0xff, 0xff },   // white
    };
    long version1, version2;
    int offset;

    version1 = Get32BE(srcBuf);
    version2 = Get32BE(srcBuf+128);
    if (version1 >= 0 && version1 <= 3) {
        offset = 0;
    } else if (version2 >= 0 && version2 <= 3 &&
        srcBuf[65] == 'P' && srcBuf[66] == 'N' && srcBuf[67] == 'T' && srcBuf[68] == 'G')
    {
        offset = 128;
    } else {
        LOGI("  MP couldn't determine picture offset!");
        goto bail;
    }
    
    pDib = new MyDIBitmap;
    if (pDib == NULL)
        goto bail;

    srcBuf += offset;
    srcBuf += kLeadingJunkCount;
    length -= offset;
    length -= kLeadingJunkCount;
    LOGI("Adjusted len is %d", length);

    outBuf = (uint8_t*) pDib->Create(kOutputWidth, kOutputHeight,
                                    1, kNumColors);
    if (outBuf == NULL) {
        delete pDib;
        pDib = NULL;
        goto bail;
    }
    pDib->SetColorTable(colorConv);

    uint8_t* outPtr;
    int line;

    /* top row goes at the bottom of the buffer */
    outPtr = outBuf + (kOutputHeight-1) * (kOutputWidth / 8);

    /*
     * Loop through all lines.  When we've output 72 bytes, stop immediately
     * even if we're in a run.  Chances are good that the run was corrupted.
     */
    for (line = 0; line < kOutputHeight; line++) {
        UnPackBits(&srcBuf, &length, &outPtr, 72, 0xff);

        /* back up to start of next line */
        outPtr -= 2*(kOutputWidth / 8);

        if (length < 0)
            break;
    }
    if (length != 0) {
        LOGI("  MP found %d unused bytes at end", length);
    }

bail:
    return pDib;
}
