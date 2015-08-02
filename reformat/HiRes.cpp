/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Reformat hi-res graphics into a bitmap.
 */
#include "StdAfx.h"
#include "HiRes.h"

/*
 * Hi-Res image format:
 *
 *  <16-bit load address>  [DOS 3.3 only]
 *  <16-bit file length>  [DOS 3.3 only]
 *  lines, in wonky format
 *
 * Each line is 40 bytes long.  Each byte holds 7 bits that define a color
 * value from 0 to 3, and the high byte determines which set of colors to
 * use.
 *
 * The colors are:
 *  0 black0    4 black1        00 00 / 80 80
 *  1 green     5 orange        2a 55 / aa d5
 *  2 purple    6 blue          55 2a / d5 aa
 *  3 white0    7 white1        7f 7f / ff ff
 *
 * There is also a "half-shift" phenomenon for the second set, which appear
 * to be shifted half a pixel to the right.  The stair-step looks like this:
 *
 *  white0
 *   purple
 *    white1
 *     blue
 *      green
 *       orange
 *
 * white1/blue/orange have a half-pixel shift, while green/orange have a
 * full-pixel shift because the colors don't start until the appropriate
 * bit (and stop on the appropriate bit).  The area between two identical
 * colors is filled.  If you have a "1 0 0 1" bit situation, a black gap
 * is formed, and a "0 1 1 0" situation creates a white area, but "0 1 0 1"
 * forms a solid color starting and stopping on the 1/280th pixels where
 * the 1s are.
 *
 * Some transition examples:
 *  purpleBBBorange
 *  purpleBBgreen
 *    blueBgreen
 *
 *  orangeWWblue
 *   greenWWpurple
 *
 * The IIgs monochrome mode is not enabled on the RGB output unless you
 * turn off AN3 by hitting $c05e (it can be re-enabled by hitting $c05f).
 * This register turns off the half-pixel shift, so it doesn't appear to
 * be possible to view hi-res output on an RGB monitor with the half-pixel
 * shift intact.  On the composite output, the removal of the half-pixel
 * shift is quite visible.
 */

/*
 * Regarding color...
 * [ Found on http://www.geocities.com/jonrelay/software/a2info/munafo.htm ]

From: munafo@gcctech.com
Newsgroups: comp.emulators.apple2,comp.sys.apple2.programmer
Subject: RGB values for the standard Apple ][ colors
Date: Thu, 12 Oct 2000 03:13:33 GMT
Lines: 137

I am giving a table of all the lores and hires colors with their values in
RGB. First, though there is a bit to explain...

[...]
Here is the table. It was used by taking the chroma/luma values and
transforming to R-Y and B-Y, then transforming to YUV and finally to RGB.
The last step requires a gamma correction for display on an RGB monitor,
I used Y to the power of -0.4. For reference, the NTSC "color bars" test
pattern colors and the YIQ axis colors are also given.

                 --chroma--
 Color name      phase ampl luma   -R- -G- -B-
 black    COLOR=0    0   0    0      0   0   0
 gray     COLOR=5    0   0   50    156 156 156
 grey     COLOR=10   0   0   50    156 156 156
 white    COLOR=15   0   0  100    255 255 255
 dk blue  COLOR=2    0  60   25     96  78 189
 lt blue  COLOR=7    0  60   75    208 195 255
 purple   COLOR=3   45 100   50    255  68 253
 purple   HCOLOR=2  45 100   50    255  68 253
 red      COLOR=1   90  60   25    227  30  96
 pink     COLOR=11  90  60   75    255 160 208
 orange   COLOR=9  135 100   50    255 106  60
 orange   HCOLOR=5 135 100   50    255 106  60
 brown    COLOR=8  180  60   25     96 114   3
 yellow   COLOR=13 180  60   75    208 221 141
 lt green COLOR=12 225 100   50     20 245  60
 green    HCOLOR=1 225 100   50     20 245  60
 dk green COLOR=4  270  60   25      0 163  96
 aqua     COLOR=14 270  60   75    114 255 208
 med blue COLOR=6  315 100   50     20 207 253
 blue     HCOLOR=6 315 100   50     20 207 253
 NTSC Hsync          0   0  -40      0   0   0
 NTSC black          0   0    7.5   41  41  41
 NTSC Gray75         0   0   77    212 212 212
 YIQ +Q             33 100   50    255  81 255
 NTSC magenta       61  82   36    255  40 181
 NTSC red          104  88   28    255  28  76
 YIQ +I            123 100   50    255  89  82
 NTSC yellow       167  62   69    221 198 121
 Color burst       180  40   0       0   4   0
 YIQ -Q            213 100   50     51 232  41
 NTSC green        241  82   48     12 234  97
 NTSC cyan         284  88   56     10 245 198
 YIQ -I            303 100   50      0 224 231
 NTSC blue         347  62   15     38  65 155

 ---

I don't think these are 100% correct, e.g. he only found one value for both
grey colors, even though one is noticeably darker than the other on-screen.

The IIgs tech note #63 uses the following for border colors:

                  Color    Color Register    Master Color
                  Name         Value            Value
                  ---------------------------------------
                  Black         $0              $0000
                  Deep Red      $1              $0D03
                  Dark Blue     $2              $0009
                  Purple        $3              $0D2D
                  Dark Green    $4              $0072
                  Dark Gray     $5              $0555
                  Medium Blue   $6              $022F
                  Light Blue    $7              $06AF
                  Brown         $8              $0850
                  Orange        $9              $0F60
                  Light Gray    $A              $0AAA
                  Pink          $B              $0F98
                  Light Green   $C              $01D0
                  Yellow        $D              $0FF0
                  Aquamarine    $E              $04F9
                  White         $F              $0FFF
                  ---------------------------------------

KEGS uses this (rearranged slightly to match the order above):
  const int g_dbhires_colors[] = {
        \* rgb *\
        0x000,      \* 0x0 black *\
        0xd03,      \* 0x1 deep red *\
        0x009,      \* 0x8 dark blue *\
        0xd0d,      \* 0x9 purple *\
        0x070,      \* 0x4 dark green *\
        0x555,      \* 0x5 dark gray *\
        0x22f,      \* 0xc medium blue *\
        0x6af,      \* 0xd light blue *\
        0x852,      \* 0x2 brown *\
        0xf60,      \* 0x3 orange *\
        0xaaa,      \* 0xa light gray *\
        0xf98,      \* 0xb pink *\
        0x0d0,      \* 0x6 green *\
        0xff0,      \* 0x7 yellow *\
        0x0f9,      \* 0xe aquamarine *\
        0xfff       \* 0xf white *\
};

The Apple values seem to be good.
*/

/*
 * Decide whether or not we want to handle this file.
 *
 * FOT with auxtype $4000 is a compressed hi-res image.  FOT with auxtype
 * $4001 is a compressed DHR image.  In practice, nobody uses these, so
 * any FOT file with a correct-looking length is treated as hi-res.
 */
void ReformatHiRes::Examine(ReformatHolder* pHolder)
{
    ReformatHolder::ReformatApplies applies = ReformatHolder::kApplicNot;
    long fileLen = pHolder->GetSourceLen(ReformatHolder::kPartData);
    long fileType = pHolder->GetFileType();
    long auxType = pHolder->GetAuxType();
    bool dosStructure =
        (pHolder->GetSourceFormat() == ReformatHolder::kSourceFormatDOS);
    bool relaxed;

    relaxed = pHolder->GetOption(ReformatHolder::kOptRelaxGfxTypeCheck) != 0;

    if (dosStructure) {
        if (fileType == kTypeBIN &&
            (fileLen >= kExpectedSize-8 && fileLen <= kExpectedSize+1) /*&&
            (auxType == 0x2000 || auxType == 0x4000)*/)
        {
            applies = ReformatHolder::kApplicProbably;
        }
    } else {
        if (fileType == kTypeFOT) {
            if (auxType == 0x8066) {
                // fhpack LZ4FH-compressed file
                applies = ReformatHolder::kApplicYes;
            } else if (fileLen >= kExpectedSize-8 &&
                       fileLen <= kExpectedSize + 1) {
                // uncompressed FOT file... probably
                applies = ReformatHolder::kApplicProbably;
            }
        } else if (relaxed && fileType == kTypeBIN &&
            (fileLen >= kExpectedSize-8 && fileLen <= kExpectedSize+1))
        {
            applies = ReformatHolder::kApplicProbably;
        }
    }

    pHolder->SetApplic(ReformatHolder::kReformatHiRes, applies,
        ReformatHolder::kApplicNot, ReformatHolder::kApplicNot);
    pHolder->SetApplic(ReformatHolder::kReformatHiRes_BW, applies,
        ReformatHolder::kApplicNot, ReformatHolder::kApplicNot);

    /*
     * Set the "preferred" flag on one option.
     */
    if (pHolder->GetOption(ReformatHolder::kOptHiResBW) == 0)
        pHolder->SetApplicPreferred(ReformatHolder::kReformatHiRes);
    else
        pHolder->SetApplicPreferred(ReformatHolder::kReformatHiRes_BW);
}

/*
 * Convert a Hi-Res image to a bitmap.
 */
int ReformatHiRes::Process(const ReformatHolder* pHolder,
    ReformatHolder::ReformatID id, ReformatHolder::ReformatPart part,
    ReformatOutput* pOutput)
{
    MyDIBitmap* pDib;
    const uint8_t* srcBuf = pHolder->GetSourceBuf(part);
    long srcLen = pHolder->GetSourceLen(part);
    int retval = -1;

    if (id == ReformatHolder::kReformatHiRes_BW) {
        fBlackWhite = true;
    }

    uint8_t expandBuf[kExpectedSize];
    if (pHolder->GetFileType() == kTypeFOT &&
        pHolder->GetAuxType() == 0x8066)
    {
        srcLen = ExpandLZ4FH(expandBuf, srcBuf, srcLen);
        if (srcLen == 0) {
            goto bail;      // fail
        }
        srcBuf = expandBuf;
    }

    if (srcLen > kExpectedSize+1 || srcLen < kExpectedSize-8) {
        LOGI(" HiRes file is not ~%d bytes long (got %d)",
            kExpectedSize, srcLen);
        goto bail;
    }

    InitLineOffset(fLineOffset);

    pDib = HiResScreenToBitmap(srcBuf);
    if (pDib == NULL)
        goto bail;

    SetResultBuffer(pOutput, pDib);
    retval = 0;

bail:
    return retval;
}

/*
 * Set up the line offset table.
 *
 * Table must be able to hold kNumLines values.
 */
/*static*/ void ReformatHiRes::InitLineOffset(int* pOffsetBuf)
{
    long offset;
    int line;

    ASSERT(pOffsetBuf != NULL);

    for (line = 0; line < kNumLines; line++) {
        offset = (line & 0x07) << 10 | (line & 0x38) << 4 |
                    (line & 0xc0) >> 1 | (line & 0xc0) >> 3;
        ASSERT(offset >= 0 && offset < (8192 - 40));
        pOffsetBuf[line] = offset;
    }
}

/*
 * (Lifted directly from fhpack.)
 *
 * Uncompress LZ4FH data from "srcBuf" to "dstBuf".  "dstBuf" must hold
 * kExpectedSize bytes.
 *
 * Returns the uncompressed length on success, 0 on failure.
 */
/*static*/ long ReformatHiRes::ExpandLZ4FH(uint8_t* outBuf,
    const uint8_t* inBuf, long inLen)
{
    // constants
    static const uint8_t LZ4FH_MAGIC = 0x66;
    static const int MIN_MATCH_LEN = 4;
    static const int INITIAL_LEN = 15;
    static const int EMPTY_MATCH_TOKEN = 253;
    static const int EOD_MATCH_TOKEN = 254;
    static const int MAX_SIZE = kExpectedSize;

    uint8_t* outPtr = outBuf;
    const uint8_t* inPtr = inBuf;

    LOGD("Expanding LZ4FH");

    if (*inPtr++ != LZ4FH_MAGIC) {
        LOGE("Missing LZ4FH magic");
        return 0;
    }

    while (true) {
        uint8_t mixedLen = *inPtr++;

        int literalLen = mixedLen >> 4;
        if (literalLen != 0) {
            if (literalLen == INITIAL_LEN) {
                literalLen += *inPtr++;
            }
            if ((outPtr - outBuf) + literalLen > MAX_SIZE ||
                    (inPtr - inBuf) + literalLen > inLen) {
                LOGE("Buffer overrun");
                return 0;
            }
            memcpy(outPtr, inPtr, literalLen);
            outPtr += literalLen;
            inPtr += literalLen;
        }

        int matchLen = mixedLen & 0x0f;
        if (matchLen == INITIAL_LEN) {
            uint8_t addon = *inPtr++;
            if (addon == EMPTY_MATCH_TOKEN) {
                matchLen = - MIN_MATCH_LEN;
            } else if (addon == EOD_MATCH_TOKEN) {
                break;      // out of while
            } else {
                matchLen += addon;
            }
        }

        matchLen += MIN_MATCH_LEN;
        if (matchLen != 0) {
            int matchOffset = *inPtr++;
            matchOffset |= (*inPtr++) << 8;
            // Can't use memcpy() here, because we need to guarantee
            // that the match is overlapping.
            uint8_t* srcPtr = outBuf + matchOffset;
            if ((outPtr - outBuf) + matchLen > MAX_SIZE ||
                    (srcPtr - outBuf) + matchLen > MAX_SIZE) {
                LOGE("Buffer overrun");
                return 0;
            }
            while (matchLen-- != 0) {
                *outPtr++ = *srcPtr++;
            }
        }
    }

    if (inPtr - inBuf != (long) inLen) {
        LOGW("Warning: LZ4FH uncompress used only %ld of %zd bytes",
                inPtr - inBuf, inLen);
    }

    return outPtr - outBuf;
}

/*
 * Convert an 8KB buffer of hi-res data to a 16-color 560x384 DIB.
 */
MyDIBitmap* ReformatHiRes::HiResScreenToBitmap(const uint8_t* buf)
{
    MyDIBitmap* pDib = new MyDIBitmap;
    uint8_t* outBuf;
    const int kLeadIn = 4;
    unsigned int colorBuf[kLeadIn+kOutputWidth +1];     // 560 half-pixels 
    int pixelBits[kPixelsPerLine];
    int shiftBits[kPixelsPerLine];
    int line;

    /* color map */
    enum {
        kColorBlack0 = 0,
        kColorGreen,
        kColorPurple,
        kColorWhite0,
        kColorBlack1,
        kColorOrange,
        kColorBlue,
        kColorWhite1,
        kColorNone,     // really only useful for debugging
        kNumColors
    };
    RGBQUAD colorConv[kNumColors];
    colorConv[0] = fPalette[kPaletteBlack];
    colorConv[1] = fPalette[kPaletteGreen];
    colorConv[2] = fPalette[kPalettePurple];
    colorConv[3] = fPalette[kPaletteWhite];
    colorConv[4] = fPalette[kPaletteBlack];
    colorConv[5] = fPalette[kPaletteOrange];
    colorConv[6] = fPalette[kPaletteMediumBlue];
    colorConv[7] = fPalette[kPaletteWhite];
    colorConv[8] = fPalette[kPaletteBlack];     // for "blank spaces"

    ASSERT(kOutputWidth == 2*kPixelsPerLine);

    if (pDib == NULL)
        goto bail;

    outBuf = (uint8_t*) pDib->Create(kOutputWidth, kOutputHeight,
                                    4, kNumColors);
    if (outBuf == NULL) {
        delete pDib;
        pDib = NULL;
        goto bail;
    }
    pDib->SetColorTable(colorConv);

    /*
     * Run through the lines.
     */
    for (line = 0; line < kNumLines; line++) {
        const uint8_t* lineData = buf + fLineOffset[line];
        int* bitPtr = pixelBits;
        int* shiftPtr = shiftBits;

        /* unravel the bits */
        for (int byt = 0; byt < kPixelsPerLine / 7; byt++) {
            uint8_t val = *lineData;
            int shifted = (val & 0x80) != 0;

            for (int bit = 0; bit < 7; bit++) {
                *bitPtr++ = val & 0x01;
                *shiftPtr++ = shifted;
                val >>= 1;
            }
            lineData++;
        }
        ASSERT(lineData <= buf + kExpectedSize);
        ASSERT((char*)bitPtr == (char*)pixelBits + sizeof(pixelBits));
        ASSERT((char*)shiftPtr == (char*)shiftBits + sizeof(shiftBits));

        /*
         * Convert the bits to colors, taking half-pixel shifts into account.
         */
        int idx;
        for (idx = 0; idx < NELEM(colorBuf); idx++)
            colorBuf[idx] = kColorNone;
        if (fBlackWhite) {
            for (idx = 0; idx < kPixelsPerLine; idx ++) {
                int bufShift = (int) shiftBits[idx];
                // simulate GS RGB by setting bufShift=0
                ASSERT(bufShift == 0 || bufShift == 1);
                int bufTarget = kLeadIn + idx * 2 + bufShift;

                if (!pixelBits[idx]) {
                    colorBuf[bufTarget] = kColorBlack0;
                    colorBuf[bufTarget+1] = kColorBlack0;
                } else {
                    colorBuf[bufTarget] = kColorWhite0;
                    colorBuf[bufTarget+1] = kColorWhite0;
                }
            }
        } else {
            for (idx = 0; idx < kPixelsPerLine; idx ++) {
                int bufShift = (int) shiftBits[idx];
                ASSERT(bufShift == 0 || bufShift == 1);
                int colorShift = 4 * bufShift;
                int bufTarget = kLeadIn + idx * 2 + bufShift;

                if (!pixelBits[idx]) {
                    colorBuf[bufTarget] = kColorBlack0 + colorShift;
                    colorBuf[bufTarget+1] = kColorBlack0 + colorShift;
                } else {
                    if (colorBuf[bufTarget-2] != kColorBlack0 &&
                        colorBuf[bufTarget-2] != kColorBlack1 &&
                        colorBuf[bufTarget-2] != kColorNone)
                    {
                        /* previous bit was set, this is white */
                        colorBuf[bufTarget] = kColorWhite0 + colorShift;
                        colorBuf[bufTarget+1] = kColorWhite0 + colorShift;

                        /* make sure the previous bit is in with us */
                        colorBuf[bufTarget-2] = kColorWhite0 + colorShift;
                        colorBuf[bufTarget-1] = kColorWhite0 + colorShift;
                    } else {
                        /* previous bit was zero, this is color */
                        if (idx & 0x01) {
                            colorBuf[bufTarget] = kColorGreen + colorShift;
                            colorBuf[bufTarget+1] = kColorGreen + colorShift;
                        } else {
                            colorBuf[bufTarget] = kColorPurple + colorShift;
                            colorBuf[bufTarget+1] = kColorPurple + colorShift;
                        }

                        /*
                         * Do we have a run of the same color?  If so, smooth
                         * the color out.  Note that white blends smoothly
                         * with everything.
                         */
                        if (colorBuf[bufTarget-4] == colorBuf[bufTarget] ||
                            colorBuf[bufTarget-4] == kColorWhite0 ||
                            colorBuf[bufTarget-4] == kColorWhite1)
                        {
                            /* back-fill previous gap with color */
                            ASSERT(colorBuf[bufTarget-2] == kColorBlack0 ||
                                   colorBuf[bufTarget-2] == kColorBlack1);
                            colorBuf[bufTarget-2] = colorBuf[bufTarget];
                            colorBuf[bufTarget-1] = colorBuf[bufTarget];
                        }
                    }
                }
            } /*for boolean pixels*/
        } /*!black&white*/

        /* convert colors to 4-bit bitmap pixels */
        /* (NOTE: should advance by GetPitch(), not assume it's equal to width) */
#define SetPix(x, y, twoval) \
        outBuf[((kOutputHeight-1) - (y)) * (kOutputWidth/2) + (x)] = twoval

        uint8_t pix4;
        for (int pix = 0; pix < kPixelsPerLine; pix++) {
            int bufPosn = kLeadIn + pix * 2;
            ASSERT(colorBuf[bufPosn] < kNumColors);
            ASSERT(colorBuf[bufPosn+1] < kNumColors);

            pix4 = colorBuf[bufPosn] << 4 | colorBuf[bufPosn+1];
            SetPix(pix, line*2, pix4);
            SetPix(pix, line*2+1, pix4);

            //SetPix(pix*2, line*2, colorBuf[bufPosn]);
            //SetPix(pix*2, line*2+1, colorBuf[bufPosn]);
            //SetPix(pix*2+1, line*2, colorBuf[bufPosn+1]);
            //SetPix(pix*2+1, line*2+1, colorBuf[bufPosn+1]);
        }
    } /*for each line*/
#undef SetPix

bail:
    return pDib;
}
