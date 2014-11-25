/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Multiple conversions for double-hi-res graphics.
 */
#include "StdAfx.h"
#include "DoubleHiRes.h"
#include "HiRes.h"

/*
 * The screen layout is described in detail in Apple //e technote #3.
 *
 * Summary: pixel data is byte-interleaved between main and auxilliary
 * 8K pages.  Seven pixels of each byte contribute to the image; the high
 * bit of each byte is ignored.  There are 16 possible colors, which match
 * up with the 16 lo-res colors.
 *
 * The interference patterns caused by adjacent colors are extremely
 * difficult to model accurately, especially considering that RGB and
 * composite outputs seem to be different.
 */

/*
 * Decide whether or not we want to handle this file.
 */
void ReformatDHR::Examine(ReformatHolder* pHolder)
{
    ReformatHolder::ReformatApplies applies = ReformatHolder::kApplicNot;
    long fileLen = pHolder->GetSourceLen(ReformatHolder::kPartData);
    long fileType = pHolder->GetFileType();
    long auxType = pHolder->GetAuxType();
    long dhrAlg;
    bool relaxed;

    relaxed = pHolder->GetOption(ReformatHolder::kOptRelaxGfxTypeCheck) != 0;

    if ((fileType == kTypeFOT && auxType < 0x4000 && fileLen == 16384) ||
        (relaxed &&
            (
            (fileType == kTypeBIN && fileLen == 16376) ||
            (fileType == kTypeBIN && fileLen == 16380) ||
            (fileType == kTypeBIN && fileLen == 16384)
            )
        ))
    {
        applies = ReformatHolder::kApplicProbably;
    }

    pHolder->SetApplic(ReformatHolder::kReformatDHR_Latched, applies,
        ReformatHolder::kApplicNot, ReformatHolder::kApplicNot);
    pHolder->SetApplic(ReformatHolder::kReformatDHR_BW, applies,
        ReformatHolder::kApplicNot, ReformatHolder::kApplicNot);
    pHolder->SetApplic(ReformatHolder::kReformatDHR_Plain140, applies,
        ReformatHolder::kApplicNot, ReformatHolder::kApplicNot);
    pHolder->SetApplic(ReformatHolder::kReformatDHR_Window, applies,
        ReformatHolder::kApplicNot, ReformatHolder::kApplicNot);

    /*
     * Set the "preferred" flag on one option.
     */
    dhrAlg = pHolder->GetOption(ReformatHolder::kOptDHRAlgorithm);

    switch ((Algorithms) dhrAlg) {
    case kDHRLatched:
        pHolder->SetApplicPreferred(ReformatHolder::kReformatDHR_Latched);
        break;
    case kDHRBlackWhite:
        pHolder->SetApplicPreferred(ReformatHolder::kReformatDHR_BW);
        break;
    case kDHRPlain140:
        pHolder->SetApplicPreferred(ReformatHolder::kReformatDHR_Plain140);
        break;
    case kDHRWindow:
        pHolder->SetApplicPreferred(ReformatHolder::kReformatDHR_Window);
        break;
    default:
        LOGI("GLITCH: DHR algorithm %d not recognized", dhrAlg);
        break;
    }
}

/*
 * Convert a Double-Hi-Res image to a bitmap.
 */
int ReformatDHR::Process(const ReformatHolder* pHolder,
    ReformatHolder::ReformatID id, ReformatHolder::ReformatPart part,
    ReformatOutput* pOutput)
{
    MyDIBitmap* pDib;
    const uint8_t* srcBuf = pHolder->GetSourceBuf(part);
    long srcLen = pHolder->GetSourceLen(part);
    int retval = -1;

    switch (id) {
    case ReformatHolder::kReformatDHR_Latched:
        fAlgorithm = kDHRLatched;
        break;
    case ReformatHolder::kReformatDHR_BW:
        fAlgorithm = kDHRBlackWhite;
        break;
    case ReformatHolder::kReformatDHR_Plain140:
        fAlgorithm = kDHRPlain140;
        break;
    case ReformatHolder::kReformatDHR_Window:
        fAlgorithm = kDHRWindow;
        break;
    default:
        LOGI("GLITCH: bad id %d", id);
        fAlgorithm = kDHRLatched;
        break;
    }

    if (srcLen > kExpectedSize || srcLen < kExpectedSize-8) {
        LOGI(" DHR file is not ~%d bytes long (got %d)",
            kExpectedSize, srcLen);
        goto bail;
    }

    /* line layout is same as standard hires */
    ReformatHiRes::InitLineOffset(fLineOffset);
    InitColorLookup();

    pDib = DHRScreenToBitmap(srcBuf);
    if (pDib == NULL)
        goto bail;

    SetResultBuffer(pOutput, pDib);
    retval = 0;

bail:
    return retval;
}

/*
 * Initialize the 4-bit-window color lookup table.
 */
void ReformatDHR::InitColorLookup(void)
{
    for (int ii = 0; ii < 4; ii++) {
        for (int jj = 0; jj < kNumDHRColors; jj++) {
            int num = jj;
            for (int kk = 0; kk < ii; kk++) {
                if (num & 0x01)
                    num |= 0x10;
                num >>= 1;
                num &= 0x0f;
            }
            fColorLookup[ii][jj] = num;
        }
    }

    return;
}

/*
 * Convert a buffer of double-hires data to a 16-color DIB.
 */
MyDIBitmap* ReformatDHR::DHRScreenToBitmap(const uint8_t* buf)
{
    MyDIBitmap* pDib = new MyDIBitmap;
    uint8_t* outBuf;
    const int kMaxLook = 4;     // padding to adjust for lookbehind/lookahead
    int pixelBits[kMaxLook+kPixelsPerLine+kMaxLook];    // 560 mono pixels
    unsigned int colorBuf[kOutputWidth];        // 560 color pixels
    int line;

    /* color map */
    enum {
        kColorBlack = 0,        // 0000
        kColorRed,              // 0001
        kColorBrown,            // 0010
        kColorOrange,           // 0011 hcolor=5
        kColorDarkGreen,        // 0100
        kColorGrey1,            // 0101
        kColorGreen,            // 0110 hcolor=1
        kColorYellow,           // 0111
        kColorDarkBlue,         // 1000
        kColorPurple,           // 1001 hcolor=2
        kColorGrey2,            // 1010
        kColorPink,             // 1011
        kColorMediumBlue,       // 1100 hcolor=6
        kColorLightBlue,        // 1101
        kColorAqua,             // 1110
        kColorWhite,            // 1110
        kNumColors
    };
    RGBQUAD colorConv[kNumColors];
    colorConv[0] =  fPalette[kPaletteBlack];
    colorConv[1] =  fPalette[kPaletteRed];
    colorConv[2] =  fPalette[kPaletteBrown];
    colorConv[3] =  fPalette[kPaletteOrange];
    colorConv[4] =  fPalette[kPaletteDarkGreen];
    colorConv[5] =  fPalette[kPaletteDarkGrey];
    colorConv[6] =  fPalette[kPaletteGreen];
    colorConv[7] =  fPalette[kPaletteYellow];
    colorConv[8] =  fPalette[kPaletteDarkBlue];
    colorConv[9] =  fPalette[kPalettePurple];
    colorConv[10] = fPalette[kPaletteLightGrey];
    colorConv[11] = fPalette[kPalettePink];
    colorConv[12] = fPalette[kPaletteMediumBlue];
    colorConv[13] = fPalette[kPaletteLightBlue];
    colorConv[14] = fPalette[kPaletteAqua];
    colorConv[15] = fPalette[kPaletteWhite];

    ASSERT(kNumColors == kPaletteSize);
    ASSERT(kNumDHRColors == kNumColors);

    ASSERT(kOutputWidth == kPixelsPerLine);

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
     *
     * This is reasonably inefficient, since I'm splitting things into
     * their constituent bits and then, for the most part, just stuffing
     * them right back together.  Someday, if fatally bored, this should
     * be optimized.
     */
    for (line = 0; line < kNumLines; line++) {
        const uint8_t* lineData = buf + fLineOffset[line];
        int* bitPtr = pixelBits + kMaxLook;

        /* this is really just to clear the fore and aft MaxLook bits */
        memset(pixelBits, 0, sizeof(pixelBits));

        /* unravel the bits */
        for (int byt = 0; byt < kPixelsPerLine / 7; byt++) {
            uint8_t val;
            
            if (byt & 0x01) {
                /* odd pixels come from main memory */
                val = *(lineData+kPageSize);
                lineData++;
            } else {
                /* even pixels come from aux mem */
                val = *lineData;
            }

            for (int bit = 0; bit < 7; bit++) {
                *bitPtr++ = val & 0x01;
                val >>= 1;
            }
        }
        ASSERT(lineData <= buf + kPageSize);
        ASSERT((char*)bitPtr == (char*)pixelBits +
            sizeof(pixelBits) - kMaxLook*sizeof(pixelBits[0]));

        /*
         * Convert the bits to colors.
         */
        int idx;
        if (fAlgorithm == kDHRBlackWhite) {
            for (idx = 0; idx < kPixelsPerLine; idx ++) {
                int bufTarget = idx;

                if (!pixelBits[idx]) {
                    colorBuf[bufTarget] = kColorBlack;
                } else {
                    colorBuf[bufTarget] = kColorWhite;
                }
            }
        } else if (fAlgorithm == kDHRPlain140) {
            /*
             * Very simple: every four pixels is a solid color.  Not too
             * close to reality, but easy to implement.
             */
            int pixVal = 0;
            bitPtr = pixelBits + kMaxLook;

            for (idx = 0; idx < kPixelsPerLine/4; idx++) {
                pixVal = *bitPtr++;
                pixVal = (pixVal << 1) | (*bitPtr++);
                pixVal = (pixVal << 1) | (*bitPtr++);
                pixVal = (pixVal << 1) | (*bitPtr++);
                colorBuf[idx*4] = pixVal;
                colorBuf[idx*4+1] = pixVal;
                colorBuf[idx*4+2] = pixVal;
                colorBuf[idx*4+3] = pixVal;
            }
            ASSERT(bitPtr == pixelBits + sizeof(pixelBits)/sizeof(pixelBits[0]) - kMaxLook);
        } else if (fAlgorithm == kDHRWindow) {
            /*
             * The way we determine the value of the color at pixel N
             * is by looking at the pixels at N-3, N-2, N-1, and N.
             *
             * We manage this with a continously shifting 4-bit-wide
             * window.
             *
             * Note to self: interesting case at line 87, pixels 200-240,
             * in "screen2".
             */
            int pixVal = 0;
            bitPtr = pixelBits + kMaxLook;

            for (idx = 0; idx < kPixelsPerLine; idx++) {
                pixVal = (pixVal << 1) & 0x0f;
                if (*bitPtr++)
                    pixVal |= 1;
                colorBuf[idx] = fColorLookup[(idx+1) & 0x03][pixVal];

            }
            ASSERT(bitPtr <= pixelBits + sizeof(pixelBits)/sizeof(pixelBits[0]));
#if 0
            /*
             * Zero-stop pass, where we set the luminosity to zero if we
             * see four zeros in a row.
             */
            bitPtr = pixelBits + kMaxLook;
            for (idx = 0; idx < kPixelsPerLine; idx++) {
                if (!bitPtr[idx] && !bitPtr[idx+1] &&
                    !bitPtr[idx+2] && !bitPtr[idx+3])
                {
                    //if (line == 87 && idx > 200 && idx < 240) {
                    //  LOGI(" %4d ERASE", idx);
                    //}
                    /*colorBuf[idx] =*/ colorBuf[idx+1] =
                        colorBuf[idx+2] = /*colorBuf[idx+3] =*/ kColorBlack;
                }
            }
#endif
        } else if (fAlgorithm == kDHRLatched) {
            /*
             * We determine the value of the color at pixel N is by looking
             * at the pixels at N-3, N-2, N-1, and N.  When we see a color
             * transition, we also look at (N+1..N+4) to special-case
             * white/black transitions.  This is necessary to reduce the
             * color fringes around sharply defined objects.
             *
             * Once a color is "latched", we keep outputting that color
             * until we find a new one that we like more.
             *
             * We manage this with a continously shifting 8-bit-wide window.
             *
             * Note to self: interesting case at line=98, pixels 50-80.
             */
            unsigned int whole;
            int newColor, oldColor;

            bitPtr = pixelBits;

            whole = 0;
            for (idx = 0; idx < 8; idx++) {
                whole <<= 1;
                if (*bitPtr++)
                    whole |= 1;
            }
            ASSERT((whole & (~0xff)) == 0);

            /* grab the color of the "previous" 4 pixels */
            oldColor = fColorLookup[idx & 0x03][whole & 0x0f];

            for (idx = 0; idx < kPixelsPerLine; idx++, bitPtr++) {
                //if (line == 98 && idx > 50 && idx < 80) {
                //  if (!(idx % 4)) {
                //      LOGI(" idx %3d: bits=0x%02x  PPPPCNNN",
                //          idx, whole & 0xff);
                //  }
                //}

                /* shift another bit in, to give us 3 prev and 4 next */
                /* looks like PPPCNNNN */
                ASSERT(*bitPtr == 0 || *bitPtr == 1);
                whole = (whole << 1) | *bitPtr;
                whole &= 0xff;      // not needed; useful for printfs

                /* get the new color (from PPPC bits) */
                newColor = fColorLookup[(idx+1) & 0x03][(whole & 0xf0) >> 4];

                //if (line == 98 && idx > 50 && idx < 80) {
                //  LOGI(" idx %3d:   old=%-2d new=%-2d (bits=0x%02x)", idx,
                //      oldColor, newColor, whole & 0xff);
                //}

                if (newColor != oldColor) {
                    /*
                     * Transition to new color; check for white/black blocks
                     * in *next* chunk of pixels.  The goal is to eliminate
                     * color fringes on white/black boundaries, which are the
                     * most easily visible.
                     */
                    int shift1, shift2, shift3;
                    shift1 = (whole >> 3) & 0x0f;   // PPCN
                    shift2 = (whole >> 2) & 0x0f;   // PCNN
                    shift3 = (whole >> 1) & 0x0f;   // CNNN

                    if (shift1 == 0x0f || shift2 == 0x0f || shift3 == 0x0f)
                        newColor = kColorWhite;
                    else if (shift1 == 0 || shift2 == 0 || shift3 == 0)
                        newColor = kColorBlack;
                    //if (line == 98 && idx > 50 && idx < 80) {
                    //  LOGI(" idx %3d:    S new=%-2d", idx, newColor);
                    //}
                }

                //if (line == 98 && idx > 50 && idx < 80) {
                    //newColor = kColorYellow;
                //}

                colorBuf[idx] = newColor;

                /*
                 * Use the new color as the old color for the next iteration.
                 * This is *NOT* the same as getting the color from PPPP
                 * before shifting next round, because we might have
                 * overridden white or black above.  In that case, the new
                 * color would be compared against white or black in the
                 * transition check, instead of comparing against the actual
                 * color of the PPPP pixels.
                 */
                oldColor = newColor;        // latch it
                //oldColor = fColorLookup[idx & 0x03][whole & 0x0f];

            }
            ASSERT(bitPtr <= pixelBits + sizeof(pixelBits)/sizeof(pixelBits[0]));
        } else {

            ASSERT(false);

#if defined(DHR_MULTIPASS)
            unsigned int colorBuf1[kMaxLook+kOutputWidth+kMaxLook];
            int pixVal = 0;
            bitPtr = pixelBits + kMaxLook;

            /*
             * Start simple.
             */
            memset(colorBuf1, 0, sizeof(colorBuf1));
            for (idx = 0; idx < kPixelsPerLine/4; idx++) {
                pixVal = *bitPtr++;
                pixVal = (pixVal << 1) | (int)(*bitPtr++);
                pixVal = (pixVal << 1) | (int)(*bitPtr++);
                pixVal = (pixVal << 1) | (int)(*bitPtr++);
                colorBuf1[idx*4 +kMaxLook] = pixVal;
                colorBuf1[idx*4+1 +kMaxLook] = pixVal;
                colorBuf1[idx*4+2 +kMaxLook] = pixVal;
                colorBuf1[idx*4+3 +kMaxLook] = pixVal;
            }
            ASSERT(bitPtr <= pixelBits + sizeof(pixelBits)/sizeof(pixelBits[0]));

            /*
             * Now go back and patch all of the transitions between colors.
             * The most significant are transitions to and from black,
             * which have to truncate bits.
             */
            for (idx = 3; idx < kMaxLook+kPixelsPerLine; idx += 4) {
                if (colorBuf1[idx] != colorBuf1[idx+1]) {
                    /* color change */
                    if (colorBuf1[idx] == 0) {
                        /* trim pixels on left */
                        pixVal = colorBuf1[idx+1];
                        ASSERT(pixVal != 0);
                        unsigned int* iptr = &colorBuf1[idx+1];
                        while (!(pixVal & 0x08)) {
                            *iptr++ = kColorBlack;
                            pixVal <<= 1;
                        }
                    } else if (colorBuf1[idx+1] == 0) {
                        /* trim pixels on right */
                        pixVal = colorBuf1[idx];
                        ASSERT(pixVal != 0);
                        unsigned int* iptr = &colorBuf1[idx];
                        while (!(pixVal & 0x01)) {
                            *iptr-- = kColorBlack;
                            pixVal >>= 1;
                        }
                    } else {
                        /*
                         * The center four pixels have a color determined
                         * like this:
                         *
                         *  orange = 0011
                         *  pink = 1011
                         *
                         * Take last two of orange and first two of pink,
                         * and swap them so first two of pink come first,
                         * e.g. 1011.  The color, which also happens to be
                         * pink, is what we set the middle four pixels to.
                         *
                         * Can't explain it, but that's how it works...
                         * usually.
                         */
                        uint8_t mergePix;

                        mergePix = colorBuf1[idx] & 0x03;
                        mergePix |= colorBuf1[idx+1] & 0x0c;
                        ASSERT((mergePix & 0xf0) == 0);
                        if (line == 191) {
                            LOGI("idx=0x%02x idx+1=0x%02x merge=0x%02x",
                                colorBuf1[idx], colorBuf1[idx+1], mergePix);
                        }
                        colorBuf1[idx-1] = colorBuf1[idx] = colorBuf1[idx+1] =
                            colorBuf1[idx+2] = mergePix;
                    }
                }
            }
            memcpy(colorBuf, colorBuf1+kMaxLook, sizeof(colorBuf));
#endif
        } /* color algorithm choices */

        /* convert colors to 4-bit bitmap pixels, with line-doubling */
#define SetPix(x, y, twoval) \
        outBuf[((kOutputHeight-1) - (y)) * (kOutputWidth/2) + (x)] = twoval

        uint8_t pix4;
        for (int pix = 0; pix < kPixelsPerLine/2; pix++) {
            int bufPosn = pix * 2;
            ASSERT(colorBuf[bufPosn] < kNumColors);
            ASSERT(colorBuf[bufPosn+1] < kNumColors);

            pix4 = colorBuf[bufPosn] << 4 | colorBuf[bufPosn+1];
            SetPix(pix, line*2, pix4);
            SetPix(pix, line*2+1, pix4);
        }
    } /*for each line*/
#undef SetPix

bail:
    return pDib;
}
