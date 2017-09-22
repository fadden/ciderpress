/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Various super-hi-res conversions, including 3200 color images.
 */
#include "StdAfx.h"
#include "SuperHiRes.h"

/*
 * ==========================================================================
 *      ReformatSHR (base class)
 * ==========================================================================
 */

/*
 * The basic Super Hi-Res "PIC" file format ($c1/0000) is just a dump of
 * memory from $2000-$9fff.
 *
 *  $2000-9cff: pixel data, 160 bytes per line
 *  $9d00-9dc7: SCB bytes (8 bits each)
 *      0-3 : color table number (0 to 15)
 *      4   : reserved, must be 0
 *      5   : fill mode (1=on, 0=off)
 *      6   : interrupts (1=generated, 0=not)
 *      7   : number of pixels (0=320, 1=640)
 *  $9e00-9fff: color tables (16 sets of 2 bytes)
 *      0-3 : blue intensity
 *      4-7 : green intensity
 *      8-11: red intensity
 *      12-15: reserved, must be 0
 *
 * Packed images ($c0/0001) are just the above packed with PackBytes.  The
 * "Apple Preferred" $c0/0002 format is funky, and has arbitrary resolution.
 *
 * 3200 color images do away with the SCB (they're always 320 mode and don't
 * support fill mode) and instead follow the pixel data with 200 sets of
 * color tables.  According to the FTN for $c1/0002 ("Brooks" format), the
 * color table is reversed, with color #15 appearing first in each table.
 */

MyDIBitmap* ReformatSHR::SHRScreenToBitmap8(const SHRScreen* pScreen)
{
    return SHRDataToBitmap8(pScreen->pixels, pScreen->scb,
                pScreen->colorTable, kPixelBytesPerLine, kNumLines,
                kOutputWidth, kOutputHeight);
}

MyDIBitmap* ReformatSHR::SHRDataToBitmap8(const uint8_t* pPixels,
    const uint8_t* pSCB, const uint8_t* pColorTable,
    unsigned int bytesPerLine, unsigned int numScanLines,
    unsigned int outputWidthPix, unsigned int outputHeightPix)
{
    MyDIBitmap* pDib = new MyDIBitmap;
    if (pDib == NULL)
        goto bail;

    // We can have an odd number of pixels, which results in 1-3 empty
    // spaces in the last byte.  We double the 4bpp pixels, so we're
    // always outputting 4 pixels per byte.
    if (outputWidthPix > bytesPerLine * 4 ||
            outputWidthPix < bytesPerLine * 4 - 3) {
        LOGW(" Invalid params: outputWidthPix=%u bytesPerLine=%u",
            outputWidthPix, bytesPerLine);
        ASSERT(false);
        goto bail;
    }

    const uint8_t* pSCBStart = pSCB;  // sanity check only

    /*
     * Set up a DIB to hold the data.  "Create" returns a pointer to the
     * pixel storage.
     */
    uint8_t* outBuf = (uint8_t*) pDib->Create(outputWidthPix, outputHeightPix,
                            8, kNumColorTables * kNumEntriesPerColorTable);
    if (outBuf == NULL) {
        delete pDib;
        pDib = NULL;
        goto bail;
    }
    int outputStride = pDib->GetPitch();

    /*
     * Convert color palette.
     */
    const uint16_t* pClrTable;
    pClrTable = (const uint16_t*) pColorTable;
    for (int table = 0; table < kNumColorTables; table++) {
        for (int entry = 0; entry < kNumEntriesPerColorTable; entry++) {
            GSColor(*pClrTable++, &fColorTables[table][entry]);
        }
    }
    pDib->SetColorTable((RGBQUAD*)fColorTables);

    if (false) {
        LOGI(" SHR color table 0");
        for (int ii = 0; ii < kNumEntriesPerColorTable; ii++) {
            LOGI("  %2d: 0x%02x %02x %02x",
                ii,
                fColorTables[0][ii].rgbRed,
                fColorTables[0][ii].rgbGreen,
                fColorTables[0][ii].rgbBlue);
        }
    }

    /*
     * Set the pixels to palette indices.
     */
    unsigned int line;
    for (line = 0; line < numScanLines; line++) {
        bool mode640, fillMode;
        int colorTableOffset;
        unsigned int byteCount;
        uint8_t pixelVal;
        uint8_t colorIndex;
        int x = 0;

        mode640 = (*pSCB & kSCBNumPixels) != 0;
        fillMode = (*pSCB & kSCBFillMode) != 0;
        colorTableOffset = (*pSCB & kSCBColorTableMask) * kNumEntriesPerColorTable;

        if (!line) {
            LOGI(" SHR line 0 mode640=%d", mode640);
        }
        if (fillMode) {
            /* I doubt anyone uses this in still images */
            LOGI(" SHR FILL MODE!!");
            DebugBreak();
        }

#define SetPix(x, y, colridx) \
        outBuf[((outputHeightPix-1) - (y)) * outputStride + (x)] = colridx

        if (mode640) {
            /* 640 mode, one byte becomes four non-doubled pixels (1:4) */
            unsigned int fullBytesPerLine = outputWidthPix / 4;
            ASSERT(fullBytesPerLine <= bytesPerLine);

            for (byteCount = 0; byteCount < fullBytesPerLine; byteCount++) {
                uint8_t pixelByte = *pPixels++;

                pixelVal = (pixelByte >> 6) & 0x03;
                colorIndex = colorTableOffset + pixelVal + 8;
                SetPix(x, line*2, colorIndex);
                SetPix(x++, line*2+1, colorIndex);

                pixelVal = (pixelByte >> 4) & 0x03;
                colorIndex = colorTableOffset + pixelVal + 12;
                SetPix(x, line*2, colorIndex);
                SetPix(x++, line*2+1, colorIndex);

                pixelVal = (pixelByte >> 2) & 0x03;
                colorIndex = colorTableOffset + pixelVal + 0;
                SetPix(x, line*2, colorIndex);
                SetPix(x++, line*2+1, colorIndex);

                pixelVal = pixelByte & 0x03;
                //rgbval = fColorLookup[colorTable][pixelVal];
                colorIndex = colorTableOffset + pixelVal + 4;
                SetPix(x, line*2, colorIndex);
                SetPix(x++, line*2+1, colorIndex);
            }
            if (byteCount != bytesPerLine) {
                // 1-3 pixels in last byte
                uint8_t pixelByte = *pPixels++;
                int rem = outputWidthPix - fullBytesPerLine * 4;
                if (rem >= 3) {
                    pixelVal = (pixelByte >> 6) & 0x03;
                    colorIndex = colorTableOffset + pixelVal + 8;
                    SetPix(x, line * 2, colorIndex);
                    SetPix(x++, line * 2 + 1, colorIndex);
                }
                if (rem >= 2) {
                    pixelVal = (pixelByte >> 4) & 0x03;
                    colorIndex = colorTableOffset + pixelVal + 12;
                    SetPix(x, line * 2, colorIndex);
                    SetPix(x++, line * 2 + 1, colorIndex);
                }
                if (rem >= 1) {
                    pixelVal = (pixelByte >> 2) & 0x03;
                    colorIndex = colorTableOffset + pixelVal + 0;
                    SetPix(x, line * 2, colorIndex);
                    SetPix(x++, line * 2 + 1, colorIndex);
                }
            }
        } else {
            /* 320 mode, one byte becomes two doubled pixels (1:4) */
            unsigned int fullBytesPerLine = outputWidthPix / 4;
            ASSERT(fullBytesPerLine <= bytesPerLine);

            for (byteCount = 0; byteCount < fullBytesPerLine; byteCount++) {
                uint8_t pixelByte = *pPixels++;

                pixelVal = (pixelByte >> 4) & 0x0f;
                colorIndex = colorTableOffset + pixelVal;
                SetPix(x, line*2, colorIndex);
                SetPix(x++, line*2+1, colorIndex);
                SetPix(x, line*2, colorIndex);
                SetPix(x++, line*2+1, colorIndex);

                pixelVal = pixelByte & 0x0f;
                colorIndex = colorTableOffset + pixelVal;
                SetPix(x, line*2, colorIndex);
                SetPix(x++, line*2+1, colorIndex);
                SetPix(x, line*2, colorIndex);
                SetPix(x++, line*2+1, colorIndex);
            }
            if (byteCount != bytesPerLine) {
                // 1 pixel in last byte
                uint8_t pixelByte = *pPixels++;
                pixelVal = (pixelByte >> 4) & 0x0f;
                colorIndex = colorTableOffset + pixelVal;
                SetPix(x, line * 2, colorIndex);
                SetPix(x++, line * 2 + 1, colorIndex);
                SetPix(x, line * 2, colorIndex);
                SetPix(x++, line * 2 + 1, colorIndex);
            }
        }
#undef SetPix

        ASSERT(x == outputWidthPix);

        pSCB++;
    }

    ASSERT(line == outputHeightPix/2);
    ASSERT(pSCB == pSCBStart + numScanLines);

bail:
    return pDib;
}


/*
 * ==========================================================================
 *      ReformatUnpackedSHR
 * ==========================================================================
 */

/*
 * Decide whether or not we want to handle this file.
 *
 * Occasionally somebody slaps the wrong aux type on, so if we're in
 * "relaxed" mode we accept just about anything that's the right size.
 */
void ReformatUnpackedSHR::Examine(ReformatHolder* pHolder)
{
    ReformatHolder::ReformatApplies applies = ReformatHolder::kApplicNot;
    long fileType = pHolder->GetFileType();
    long auxType = pHolder->GetAuxType();
    long fileLen = pHolder->GetSourceLen(ReformatHolder::kPartData);
    const char* nameExt = pHolder->GetNameExt();
    bool relaxed;

    relaxed = pHolder->GetOption(ReformatHolder::kOptRelaxGfxTypeCheck) != 0;

    /* uncompressed $c1/0000, but many variations on aux type */
    if ((fileType == kTypePIC && auxType == 0x0000 && fileLen == 32768) ||
        (relaxed &&
            (
            (fileType == kTypePIC && fileLen == 32768) ||
            (fileType == kTypeBIN && fileLen == 32768 &&
                (stricmp(nameExt, ".PIC") == 0 || stricmp(nameExt, ".SHR") == 0))
            )
        ))
    {
        applies = ReformatHolder::kApplicProbably;
    }

    pHolder->SetApplic(ReformatHolder::kReformatSHR_PIC, applies,
        ReformatHolder::kApplicNot, ReformatHolder::kApplicNot);
}

/*
 * Convert an unpacked Super Hi-Res image.
 */
int ReformatUnpackedSHR::Process(const ReformatHolder* pHolder,
    ReformatHolder::ReformatID id, ReformatHolder::ReformatPart part,
    ReformatOutput* pOutput)
{
    MyDIBitmap* pDib;
    int retval = -1;

    if (pHolder->GetSourceLen(part) != kTotalSize) {
        LOGI(" SHR file is not %d bytes long!", kTotalSize);
        goto bail;
    }

    ASSERT(sizeof(SHRScreen) == kTotalSize);

    memcpy(&fScreen, pHolder->GetSourceBuf(part), sizeof(fScreen));

    pDib = SHRScreenToBitmap8(&fScreen);
    if (pDib == NULL)
        goto bail;

    SetResultBuffer(pOutput, pDib);
    retval = 0;

bail:
    return retval;
}


/*
 * ==========================================================================
 *      ReformatAppSHR
 * ==========================================================================
 */

/*
 * Decide whether or not we want to handle this file.
 *
 * This file type seems exclusive to the IIgs version of "John Elway
 * Quarterback".
 */
void ReformatJEQSHR::Examine(ReformatHolder* pHolder)
{
    ReformatHolder::ReformatApplies applies = ReformatHolder::kApplicNot;
    long fileType = pHolder->GetFileType();
    long auxType = pHolder->GetAuxType();
    long fileLen = pHolder->GetSourceLen(ReformatHolder::kPartData);
    const char* nameExt = pHolder->GetNameExt();
    bool relaxed;

    relaxed = pHolder->GetOption(ReformatHolder::kOptRelaxGfxTypeCheck) != 0;

    /* uncompressed $c1/0000, but many variations on aux type */
    if (fileType == kTypeBIN && auxType == 0x0000 &&
        fileLen == kExpectedLen && stricmp(nameExt, ".APP") == 0)
    {
        applies = ReformatHolder::kApplicProbably;
    }

    pHolder->SetApplic(ReformatHolder::kReformatSHR_JEQ, applies,
        ReformatHolder::kApplicNot, ReformatHolder::kApplicNot);
}

/*
 * Convert one of these odd-format images.  It appears to be a dump of the
 * SHR screen minus the "reserved" section between the SCB and color table,
 * and only one color table is stored.  Total savings of 480 bytes -- less
 * than a ProDOS block.  Sorta dumb.
 */
int ReformatJEQSHR::Process(const ReformatHolder* pHolder,
    ReformatHolder::ReformatID id, ReformatHolder::ReformatPart part,
    ReformatOutput* pOutput)
{
    MyDIBitmap* pDib;
    const uint8_t* srcBuf = pHolder->GetSourceBuf(part);
    long srcLen = pHolder->GetSourceLen(part);
    int retval = -1;

    if (pHolder->GetSourceLen(part) != kExpectedLen) {
        LOGI(" SHR file is not %d bytes long!", kTotalSize);
        goto bail;
    }

    ASSERT(sizeof(SHRScreen) == kTotalSize);

    memcpy(&fScreen.pixels, srcBuf, 32000);
    memcpy(&fScreen.scb, srcBuf + 0x7d00, 256);
    memcpy(&fScreen.colorTable, srcBuf + 0x7e00, 32);

    pDib = SHRScreenToBitmap8(&fScreen);
    if (pDib == NULL)
        goto bail;

    SetResultBuffer(pOutput, pDib);
    retval = 0;

bail:
    return retval;
}


/*
 * ==========================================================================
 *      ReformatPaintworksSHR
 * ==========================================================================
 */

/*
 * Decide whether or not we want to handle this file.
 */
void ReformatPaintworksSHR::Examine(ReformatHolder* pHolder)
{
    ReformatHolder::ReformatApplies applies = ReformatHolder::kApplicNot;
    long fileType = pHolder->GetFileType();
    long auxType = pHolder->GetAuxType();
    //long fileLen = pHolder->GetSourceLen(ReformatHolder::kPartData);
    //const char* nameExt = pHolder->GetNameExt();
    //bool relaxed;

    //relaxed = pHolder->GetOption(ReformatHolder::kOptRelaxGfxTypeCheck) != 0;

    /* PNT $c0/0000 */
    if (fileType == kTypePNT && auxType == 0x0000)
        applies = ReformatHolder::kApplicProbably;

    pHolder->SetApplic(ReformatHolder::kReformatSHR_Paintworks, applies,
        ReformatHolder::kApplicNot, ReformatHolder::kApplicNot);
}

/*
 * Convert a PaintWorks format Super Hi-Res image.
 *
 * The format is loosely documented in the file type note for $C0/0000:
 *  +$000 to +$01F: Bytes
 *    Super Hi-Res Palette
 *  +$020 to +$021: Word
 *    Background color
 *  +$022 to +$221: Bytes
 *    Patterns. 16 QuickDraw II patterns, each 32 bytes in length.
 *  +$222 to EOF: Bytes
 *    Packed graphics data. Note that the unpacked data could be longer
 *    than one Super Hi-Res screen (Paintworks allows full-page sized
 *    documents). 
 *
 * Sometimes it runs a few bytes over.  If it runs significantly under, it's
 * probably a generic packed image (PNT/0001) with the wrong auxtype.
 */
int ReformatPaintworksSHR::Process(const ReformatHolder* pHolder,
    ReformatHolder::ReformatID id, ReformatHolder::ReformatPart part,
    ReformatOutput* pOutput)
{
    MyDIBitmap* pDib;
    const int kPWOutputLines = 396;
    const int kPWOutputSize = kPWOutputLines * kPixelBytesPerLine;
    const int kPWDataOffset = 0x222;
    uint8_t scb[kPWOutputLines];
    uint8_t colorTable[kNumEntriesPerColorTable * kColorTableEntrySize];
    uint8_t* unpackBuf = NULL;
    int retval = -1;
    int i, result;

    if (pHolder->GetSourceLen(part) < kMinSize) {
        LOGW(" SHR file too short (%ld)", pHolder->GetSourceLen(part));
        goto bail;
    }

    /*
     * Copy the color table out.
     */
    memcpy(colorTable, pHolder->GetSourceBuf(part), sizeof(colorTable));

    /*
     * Unpack the packed data.
     *
     * For some reason it's fairly common to have exactly 9 bytes left over
     * in the input.  It appears that these files have 400 lines of graphics,
     * and the first 8 bytes are PackBytes for the 4 lines.  The very last
     * byte is a zero, for no apparent reason.  Best guess is something other
     * than PaintWorks wrote these, because SuperConvert expects them to
     * have 396 lines and only recognizes that many.
     */
    unpackBuf = new uint8_t[kPWOutputSize];
    if (unpackBuf == NULL)
        goto bail;
    memset(unpackBuf, 0, sizeof(unpackBuf));        // in case we fall short

    result = UnpackBytes(unpackBuf,
                pHolder->GetSourceBuf(part) + kPWDataOffset,
                kPWOutputSize,
                pHolder->GetSourceLen(part) - kPWDataOffset);
    if (result != kPWOutputSize) {
        LOGI("WARNING: UnpackBytes wasn't happy");

#if 0
        /* thd282.shk (rev76.2) has a large collection of these */
        if (UnpackBytes((uint8_t*) &fScreen,
            pHolder->GetSourceBuf(part),
            kTotalSize,
            pHolder->GetSourceLen(part)) == 0)
        {
            pDib = SHRScreenToBitmap8(&fScreen);
            if (pDib == NULL)
                goto bail;
            goto gotit;
        }
#endif
    }

    for (i = 0; i < kPWOutputLines; i++)
        scb[i] = 0;     // 320 mode
    pDib = SHRDataToBitmap8(unpackBuf, scb, colorTable, kPixelBytesPerLine,
            kPWOutputLines, kOutputWidth, kPWOutputLines * 2);
    if (pDib == NULL)
        goto bail;

//gotit:
    SetResultBuffer(pOutput, pDib);
    retval = 0;

bail:
    delete[] unpackBuf;
    return retval;
}


/*
 * ==========================================================================
 *      ReformatPackedSHR
 * ==========================================================================
 */

/*
 * Decide whether or not we want to handle this file.
 */
void ReformatPackedSHR::Examine(ReformatHolder* pHolder)
{
    ReformatHolder::ReformatApplies applies = ReformatHolder::kApplicNot;
    long fileType = pHolder->GetFileType();
    long auxType = pHolder->GetAuxType();
    //long fileLen = pHolder->GetSourceLen(ReformatHolder::kPartData);
    //const char* nameExt = pHolder->GetNameExt();
    //bool relaxed;

    //relaxed = pHolder->GetOption(ReformatHolder::kOptRelaxGfxTypeCheck) != 0;

    /* compressed $c0/0001 */
    if (fileType == kTypePNT && auxType == 0x0001)
        applies = ReformatHolder::kApplicProbably;

    pHolder->SetApplic(ReformatHolder::kReformatSHR_Packed, applies,
        ReformatHolder::kApplicNot, ReformatHolder::kApplicNot);
}

/*
 * Convert a packed Super Hi-Res image (PNT/$0001).
 */
int ReformatPackedSHR::Process(const ReformatHolder* pHolder,
    ReformatHolder::ReformatID id, ReformatHolder::ReformatPart part,
    ReformatOutput* pOutput)
{
    MyDIBitmap* pDib;
    int retval = -1;

    if (pHolder->GetSourceLen(part) < 4) {
        LOGW(" SHR file too short (%ld)", pHolder->GetSourceLen(part));
        goto bail;
    }

    ASSERT(sizeof(SHRScreen) == kTotalSize);

    if (UnpackBytes((uint8_t*) &fScreen,
        pHolder->GetSourceBuf(part), kTotalSize,
        pHolder->GetSourceLen(part)) != kTotalSize)
    {
        LOGW(" SHR UnpackBytes failed");
        goto bail;
    }

    pDib = SHRScreenToBitmap8(&fScreen);
    if (pDib == NULL)
        goto bail;

    SetResultBuffer(pOutput, pDib);
    retval = 0;

bail:
    return retval;
}


/*
 * ==========================================================================
 *      ReformatAPFSHR
 * ==========================================================================
 */

/*
 * Decide whether or not we want to handle this file.
 */
void ReformatAPFSHR::Examine(ReformatHolder* pHolder)
{
    ReformatHolder::ReformatApplies applies = ReformatHolder::kApplicNot;
    long fileType = pHolder->GetFileType();
    long auxType = pHolder->GetAuxType();
    //long fileLen = pHolder->GetSourceLen(ReformatHolder::kPartData);
    //const char* nameExt = pHolder->GetNameExt();
    //bool relaxed;

    //relaxed = pHolder->GetOption(ReformatHolder::kOptRelaxGfxTypeCheck) != 0;

    /* compressed $c0/0002 */
    if (fileType == kTypePNT && auxType == 0x0002)
        applies = ReformatHolder::kApplicProbably;

    pHolder->SetApplic(ReformatHolder::kReformatSHR_APF, applies,
        ReformatHolder::kApplicNot, ReformatHolder::kApplicNot);
}

/*
 * Convert a Super Hi-Res image in APF format (PNT/$0002).
 *
 * Blocks are a 32-bit length followed by a pascal string describing the
 * contents.  Common blocks are "MAIN", "PATS", and "SCIB", but could be
 * anything (e.g. "Platinum Paint").
 *
 * All we're really interested in is the "MAIN" block, though we need
 * to handle MULTIPAL as well for 3200-color images.
 */
int ReformatAPFSHR::Process(const ReformatHolder* pHolder,
    ReformatHolder::ReformatID id, ReformatHolder::ReformatPart part,
    ReformatOutput* pOutput)
{
    MyDIBitmap* pDib;
    const uint8_t* srcPtr = pHolder->GetSourceBuf(part);
    long srcLen = pHolder->GetSourceLen(part);
    uint8_t multiPal[kNumLines *
                        kNumEntriesPerColorTable * kColorTableEntrySize];
    bool is3200 = false;
    bool haveGraphics = false;
    int retval = -1;

    if (srcLen < 4) {
        LOGW(" SHR file too short (%ld)", srcLen);
        goto bail;
    }

    ASSERT(sizeof(SHRScreen) == kTotalSize);

    while (srcLen > 0) {
        CString blockName;
        const uint8_t* nextBlock;
        long blockLen, dataLen, nextLen;
        int nameLen;

        blockLen = Read32(&srcPtr, &srcLen);
        if (blockLen > srcLen + 4) {
            LOGI(" APFSHR WARNING: found blockLen=%ld, remaining len=%ld",
                blockLen, srcLen);
            break;
            //goto bail;
        }
        nextBlock = srcPtr + (blockLen-4);
        nextLen = srcLen - (blockLen-4);

        nameLen = ::GetPascalString(srcPtr, srcLen, &blockName);
        if (nameLen < 0) {
            LOGI(" APFSHR failed getting pascal name, bailing");
            goto bail;
        }

        srcPtr += nameLen+1;
        srcLen -= nameLen+1;

        dataLen = blockLen - (nameLen+1 + 4);

        LOGI(" APFSHR block='%ls' blockLen=%ld (dataLen=%ld) start=0x%p",
            (LPCWSTR) blockName, blockLen, dataLen, srcPtr);

        if (blockName == "MAIN") {
            if (UnpackMain(srcPtr, dataLen) != 0)
                goto bail;
            haveGraphics = true;
        } else if (blockName == "MULTIPAL") {
            if (UnpackMultipal(multiPal, srcPtr, dataLen) == 0)
                is3200 = true;
        } else if (blockName == "NOTE") {
            UnpackNote(srcPtr, dataLen);
        } else {
            LOGI(" APFSHR  (ignoring segment '%ls')", (LPCWSTR) blockName);
        }

        srcPtr = nextBlock;
        srcLen = nextLen;
    }

    if (!haveGraphics)
        goto bail;

    // TODO: we don't currently handle MULTIPAL with anything other than
    //  a 320x200 image.  Fortunately this is rare.
    if (is3200 && !fNonStandard) {
        Reformat3200SHR ref3200(&fScreen, multiPal);

        pDib = ref3200.SHR3200ToBitmap24();
    } else if (fNonStandard) {
        pDib = SHRDataToBitmap8(fPixelStore, fSCBStore,
                fScreen.colorTable, fPixelBytesPerLine, fNumScanLines,
                fOutputWidth, fOutputHeight);

    } else {
        pDib = SHRScreenToBitmap8(&fScreen);
    }
    if (pDib == NULL)
        goto bail;

    SetResultBuffer(pOutput, pDib);
    retval = 0;

bail:
    return retval;
}

/*
 * Unpack the "MAIN" segment of an APF paint file into fScreen.
 *
 *  +$00    MasterMode: 16-bit ModeWord value for MasterSCB.  A ModeWord
 *            with a high byte of zero has the SCB in the low byte.
 *  +$02    PixelsPerScanLine: 16-bit value, usually 320 or 640 but may
 *            be something different.
 *  +$04    NumColorTables: 16-bit value, should be between 0 and 15
 *  +$06    ColorTableArray: [0..NumColorTables-1] of ColorTable, where
 *            a ColorTable is 16 sets of 16-bit $0RGB values.
 *  +$xx    NumScanLines: 16-bit count of scan lines, often 200.
 *  +$xx+2  ScanLineDirectory: [0..NumScanLines-1] of DirEntry.  Each
 *            DirEntry has two parts, a 16-bit count of packed-data-length
 *            for each line, followed by a 16-bit ModeWord value.
 *  +$yy    PackedScanLines: [0..NumScanLines-1] of packed data.  Each
 *            scan line is individually packed with PackBytes.
 *
 * Returns 0 on success, -1 on failure.
 *
 * Question: if the image is 640x200, is it in 640 mode?  Can we depend on
 * the MasterMode word to tell us if it's a 320-mode 4-bit-per-pixel image
 * that happens to be 640 pixels wide?  Did all //gs applications set this
 * correctly?  Can the format be different on every line?
 */
int ReformatAPFSHR::UnpackMain(const uint8_t* srcPtr, long srcLen)
{
    const int kColorTableSize = kNumEntriesPerColorTable * kColorTableEntrySize;
    uint16_t masterMode;
    unsigned int numColorTables;
    uint16_t* packedDataLen = NULL;
    int retval = -1;

    if (srcLen < 64) {
        /* can't possibly be this small */
        LOGI(" APFSHR unlikely srcLen %d", srcLen);
        goto bail;
    }

    masterMode = Read16(&srcPtr, &srcLen);
    fPixelsPerScanLine = Read16(&srcPtr, &srcLen);
    numColorTables = Read16(&srcPtr, &srcLen);
    if (fPixelsPerScanLine == 0 || fPixelsPerScanLine > kMaxPixelsPerScan) {
        LOGI(" APFSHR unsupported pixelsPerScanLine %d",
            fPixelsPerScanLine);
        goto bail;
    }

    LOGD(" APFSHR  masterMode=0x%04x, ppsl=%d, nct=%d",
        masterMode, fPixelsPerScanLine, numColorTables);
    if (numColorTables == 0 || numColorTables > kNumColorTables) {
        // Zero is valid per the spec, but we don't know how to render that.
        // Rather than showing an all-black bitmap, just fail.
        LOGI(" APFSHR unsupported numColorTables %d", numColorTables);
        goto bail;
    }

    /*
     * Load the color tables.
     */
    memset(fScreen.colorTable, 0, sizeof(fScreen.colorTable));
    for (unsigned int i = 0; i < numColorTables; i++) {
        if (srcLen < kColorTableSize) {
            LOGI(" APFSHR underrun while copying color tables");
            goto bail;
        }

        memcpy(&fScreen.colorTable[i * kColorTableSize], srcPtr,
            kColorTableSize);
        srcPtr += kColorTableSize;
        srcLen -= kColorTableSize;
    }

    /*
     * Get the scanline count.
     *
     * fOutputHeight is twice the #of scan lines because we use pixel
     * doubling so we can handle both 320 mode and 640 mode.
     */
    fNumScanLines = Read16(&srcPtr, &srcLen);
    if (fNumScanLines == 0 || fNumScanLines > kMaxScanLines) {
        LOGI(" APFSHR unsupported numScanLines %d", fNumScanLines);
        goto bail;
    }
    if ((fPixelsPerScanLine == 320 || fPixelsPerScanLine == 640) &&
        fNumScanLines == kNumLines)
    {
        /* standard-sized image, use fScreen */
        ASSERT(!fNonStandard);
        LOGD("  Assuming this is a standard, full-width SHR image");
        fPixelBytesPerLine = kPixelBytesPerLine;        // 160
        fPixelStore = fScreen.pixels;
        fSCBStore = fScreen.scb;
        // These don't matter if "fNonStandard" is false, but set them anyway
        // so the debug output looks okay.
        fOutputWidth = 640;
        fOutputHeight = kNumLines * 2;
    } else {
        /* non-standard image, allocate storage */
        fNonStandard = true;
        LOGI("  NOTE: non-standard image size %dx%d, 2-bit-mode=%d",
            fPixelsPerScanLine, fNumScanLines,
            (masterMode & kSCBNumPixels) != 0);

        fOutputHeight = fNumScanLines * 2;
        if ((masterMode & kSCBNumPixels) == 0) {
            fOutputWidth = fPixelsPerScanLine * 2;
            fPixelBytesPerLine = (fPixelsPerScanLine + 1) / 2;  // "320 mode"
        } else {
            fOutputWidth = fPixelsPerScanLine;
            fPixelBytesPerLine = (fPixelsPerScanLine + 3) / 4;  // "640 mode"
        }
        fPixelStore = new uint8_t[fPixelBytesPerLine * fNumScanLines];
        if (fPixelStore == NULL) {
            LOGI(" APFSHR ERROR: alloc of %d bytes fPixelStore failed",
                fPixelBytesPerLine * fNumScanLines);
            goto bail;
        }
        fSCBStore = new uint8_t[fNumScanLines];
        if (fSCBStore == NULL) {
            LOGI(" APFSHR ERROR: alloc of %d bytes fSCBStore failed",
                fNumScanLines);
            goto bail;
        }
    }
    LOGD(" APFSHR  numScanLines=%d, outputWidth=%d, pixelBytesPerLine=%d",
        fNumScanLines, fOutputWidth, fPixelBytesPerLine);

    /*
     * Get the per-scanline data.
     */
    packedDataLen = new uint16_t[fNumScanLines];
    if (packedDataLen == NULL)
        goto bail;
    for (unsigned int i = 0; i < fNumScanLines; i++) {
        packedDataLen[i] = Read16(&srcPtr, &srcLen);
        if (packedDataLen[i] > fPixelsPerScanLine) {
            /* each pixel is 2 or 4 bits, so this is a 2-4x expansion */
            LOGI(" APFSHR got funky packed len %d for line %d",
                packedDataLen[i], i);
            goto bail;
        }

        uint16_t mode = Read16(&srcPtr, &srcLen);
        if (mode >> 8 == 0)
            fSCBStore[i] = (uint8_t) mode;
        else {
            LOGI(" APFSHR odd mode 0x%04x on line %d", mode, i);
            fSCBStore[i] = 0;   // ?
        }
    }

    /*
     * Unpack the scan lines into the pixel storage.  Some APF files
     * use PackBytes in a not-quite-safe way, e.g. a 28x30 image with
     * 14 bytes per line uses the 32-bit pattern repeat mode (0x3fff),
     * yielding 16 bytes.  We need to unpack to a temp buffer.
     *
     * I've found a few from 816/Paint that appear to have the wrong width
     * value; they claim 320 but look like they're actually 640 (the image
     * seems to be cut in half, and we unpack 2x as much data as we expect).
     */
    for (unsigned int i = 0; i < fNumScanLines; i++) {
        // PackBytes can generate 256 bytes from a single pair of bytes.
        uint8_t tmpBuf[ReformatSHR::kMaxPixelsPerScan];

        if (srcLen <= 0) {
            LOGI(" APFSHR ran out of data while unpacking pixels");
            goto bail;
        }
        int actual = UnpackBytes(tmpBuf, srcPtr, sizeof(tmpBuf), packedDataLen[i]);
        if (actual < (int) fPixelBytesPerLine) {  // includes actual < 0
            LOGI(" APFSHR UnpackBytes failed on line %d (pbpl=%d actual=%d)",
                i, fPixelBytesPerLine, actual);
            goto bail;
        }
        //LOGD("+++ unpackbytes %d pbpl=%d pdl=%d --> %d",
        //    i, fPixelBytesPerLine, packedDataLen[i], actual);
        memcpy(&fPixelStore[i * fPixelBytesPerLine], tmpBuf, fPixelBytesPerLine);

        srcPtr += packedDataLen[i];
        srcLen -= packedDataLen[i];
    }

    retval = 0;

bail:
    delete[] packedDataLen;
    return retval;
}

/*
 * Unpack the "multipal" data.
 *
 * Unfortunately the guy who wrote "SuperView" decided to un-swap the
 * Brooks format by stuffing them into the file the wrong way.  We could
 * fix it here, but we can't reliably detect the files.
 */
int ReformatAPFSHR::UnpackMultipal(uint8_t* dstPtr,
    const uint8_t* srcPtr, long srcLen)
{
    const int kMultipalSize = kNumLines *
                        kNumEntriesPerColorTable * kColorTableEntrySize;
    int numColorTables;

    /* check the size; use (size+2) to factor in 16-bit count */
    if (srcLen < kMultipalSize+2) {
        LOGI("  APFSHR got too-small multipal size %ld", srcLen);
        return -1;
    } else if (srcLen > kMultipalSize+2) {
        LOGI("  APFSHR WARNING: oversized multipal (%ld, expected %ld)",
            srcLen, kMultipalSize);
        /* keep going */
    }

    /* get the #of color tables; should always be 200 */
    numColorTables = Read16(&srcPtr, &srcLen);
    if (numColorTables != kNumLines) {
        /* expecting one palette per line */
        LOGI("  APFSHR ignoring multipal with %d color tables",
            numColorTables);
        return -1;
    }

    if (1) {
        memcpy(dstPtr, srcPtr, kMultipalSize);
    } else {
#if 0
        /* swap entries */
        const uint16_t* pSrcTable;
        uint16_t* pDstTable = (uint16_t*) dst;
        int table, entry;
        for (table = 0; table < kNumLines; table++) {
            pSrcTable = (const uint16_t*)src +
                        (kNumLines - table) * kNumEntriesPerColorTable;
            for (entry = 0; entry < kNumEntriesPerColorTable; entry++) {
                pDstTable[entry] = *pSrcTable++;
            }
            pDstTable += kNumEntriesPerColorTable;
        }
#endif
    }

    return 0;
}

/*
 * Unpack a "NOTE" chunk.  This seems to be a 16-bit count followed by
 * ASCII data.
 */
void ReformatAPFSHR::UnpackNote(const uint8_t* srcPtr, long srcLen)
{
    int numChars;

    numChars = Read16(&srcPtr, &srcLen);

    if (numChars != srcLen) {
        LOGI("  APFSHR note chunk has numChars=%d but dataLen=%ld, bailing",
            numChars, srcLen);
        return;
    }

    CString str;
    while (srcLen--) {
        str += *srcPtr++;
    }

    LOGI(" APFSHR   note: '%ls'", (LPCWSTR) str);
}


/*
 * ==========================================================================
 *      Reformat3200SHR
 * ==========================================================================
 */

/*
 * Decide whether or not we want to handle this file.
 */
void Reformat3200SHR::Examine(ReformatHolder* pHolder)
{
    ReformatHolder::ReformatApplies applies = ReformatHolder::kApplicNot;
    long fileType = pHolder->GetFileType();
    long auxType = pHolder->GetAuxType();
    long fileLen = pHolder->GetSourceLen(ReformatHolder::kPartData);
    const char* nameExt = pHolder->GetNameExt();
    bool relaxed;

    relaxed = pHolder->GetOption(ReformatHolder::kOptRelaxGfxTypeCheck) != 0;

    /* should be $c1/0002, but there are many variations */
    if ((fileType == kTypePIC && auxType == 0x0002 && fileLen == 38400) ||
        (fileType == kTypeBIN && fileLen == 38400 &&
                stricmp(nameExt, ".3200") == 0) ||
        (relaxed &&
            (
            (fileType == kTypePIC && fileLen == 38400)
            )
        ))
    {
        applies = ReformatHolder::kApplicProbably;
    }

    pHolder->SetApplic(ReformatHolder::kReformatSHR_3200, applies,
        ReformatHolder::kApplicNot, ReformatHolder::kApplicNot);
}

/*
 * Convert a 3200-color Super Hi-Res Image.
 */
int Reformat3200SHR::Process(const ReformatHolder* pHolder,
    ReformatHolder::ReformatID id, ReformatHolder::ReformatPart part,
    ReformatOutput* pOutput)
{
    MyDIBitmap* pDib;
    int retval = -1;

    if (pHolder->GetSourceLen(part) != kExtTotalSize) {
        LOGI(" SHR file is not %d bytes long!", kExtTotalSize);
        return retval;
    }

    ASSERT(sizeof(fScreen.pixels) + sizeof(fExtColorTable) == kExtTotalSize);

    ASSERT(sizeof(fScreen.pixels) == 32000);
    ASSERT(sizeof(fExtColorTable) == 32 * 200);

    memcpy(fScreen.pixels, pHolder->GetSourceBuf(part), sizeof(fScreen.pixels));
    //memcpy(fExtColorTable, (*ppBuf) + sizeof(fScreen.pixels),
    //  sizeof(fExtColorTable));

    /* "Brooks" format color tables are stored in reverse order */
    const uint16_t* pSrcTable = (const uint16_t*)
                        (pHolder->GetSourceBuf(part) + sizeof(fScreen.pixels));
    uint16_t* pDstTable = (uint16_t*) fExtColorTable;
    for (int table = 0; table < kExtNumColorTables; table++) {
        int entry;
        for (entry = 0; entry < kNumEntriesPerColorTable; entry++) {
            pDstTable[(kNumEntriesPerColorTable-1) - entry] = *pSrcTable++;
        }
        pDstTable += kNumEntriesPerColorTable;
    }

    pDib = SHR3200ToBitmap24();
    if (pDib == NULL)
        goto bail;

    SetResultBuffer(pOutput, pDib);
    retval = 0;

bail:
    return retval;
}

/*
 * Convert a SHRScreen struct to a 24-bit DIB, using a different
 * color palette for each line.
 *
 * This is shared among the 3200-color SHR formats.
 */
MyDIBitmap* Reformat3200SHR::SHR3200ToBitmap24(void)
{
    MyDIBitmap* pDib = new MyDIBitmap;
    RGBTRIPLE colorLookup[kExtNumColorTables][kNumEntriesPerColorTable];
    RGBTRIPLE* rgbBuf;
    const uint8_t* pPixels = fScreen.pixels;

    if (pDib == NULL)
        goto bail;

    /*
     * Set up a DIB to hold the data.
     */
    rgbBuf = (RGBTRIPLE*) pDib->Create(kOutputWidth, kOutputHeight, 24, 0);
    if (rgbBuf == NULL) {
        delete pDib;
        pDib = NULL;
        goto bail;
    }

    /*
     * Convert color palette data to RGBTRIPLE form.
     */
    const uint16_t* pClrTable;
    pClrTable = (const uint16_t*) fExtColorTable;
    for (int table = 0; table < kExtNumColorTables; table++) {
        for (int entry = 0; entry < kNumEntriesPerColorTable; entry++) {
            GSColor(*pClrTable++, &colorLookup[table][entry]);
            //if (!table) {
            //  LOGI(" table %2d entry %2d value=0x%04x",
            //      table, entry, *pClrTable);
            //}
        }
    }

    /*
     * Set the pixels in our private RGB buffer.
     */
    int line;
    for (line = 0; line < kNumLines; line++) {
        int byteCount, pixelByte;
        uint8_t pixelVal;
        RGBTRIPLE rgbval;
        int x = 0;

#define SetPix(x, y, rgbval) \
        rgbBuf[((kOutputHeight-1) - (y)) * kOutputWidth + (x)] = rgbval

        for (byteCount = 0; byteCount < kPixelBytesPerLine; byteCount++) {
            pixelByte = *pPixels++;

            pixelVal = (pixelByte >> 4) & 0x0f;
            rgbval = colorLookup[line][pixelVal];
            SetPix(x, line*2, rgbval);
            SetPix(x++, line*2+1, rgbval);
            SetPix(x, line*2, rgbval);
            SetPix(x++, line*2+1, rgbval);

            pixelVal = pixelByte & 0x0f;
            rgbval = colorLookup[line][pixelVal];
            SetPix(x, line*2, rgbval);
            SetPix(x++, line*2+1, rgbval);
            SetPix(x, line*2, rgbval);
            SetPix(x++, line*2+1, rgbval);
        }
#undef SetPix

        ASSERT(x == kOutputWidth);
    }
    ASSERT(line == kOutputHeight/2);

bail:
    return pDib;
}


/*
 * ==========================================================================
 *      Reformat3201SHR
 * ==========================================================================
 */

/*
 * Decide whether or not we want to handle this file.
 *
 * This *might* also be $c0/0004, but there's no file type note
 * for that.
 */
void Reformat3201SHR::Examine(ReformatHolder* pHolder)
{
    ReformatHolder::ReformatApplies applies = ReformatHolder::kApplicNot;
    long fileType = pHolder->GetFileType();
    long auxType = pHolder->GetAuxType();
    //long fileLen = pHolder->GetSourceLen(ReformatHolder::kPartData);
    const char* nameExt = pHolder->GetNameExt();
    //bool relaxed;

    //relaxed = pHolder->GetOption(ReformatHolder::kOptRelaxGfxTypeCheck) != 0;

    /* only identified by name */
    if (stricmp(nameExt, ".3201") == 0)
        applies = ReformatHolder::kApplicProbably;

    pHolder->SetApplic(ReformatHolder::kReformatSHR_3201, applies,
        ReformatHolder::kApplicNot, ReformatHolder::kApplicNot);
}

/*
 * Convert a 3201-format packed 3200-color Super Hi-Res Image.
 */
int Reformat3201SHR::Process(const ReformatHolder* pHolder,
    ReformatHolder::ReformatID id, ReformatHolder::ReformatPart part,
    ReformatOutput* pOutput)
{
    MyDIBitmap* pDib;
    const uint8_t* srcBuf = pHolder->GetSourceBuf(part);
    long srcLen = pHolder->GetSourceLen(part);
    long length = srcLen;
    uint8_t* tmpBuf = NULL;
    int retval = -1;

    if (srcLen < 16 || srcLen > kExtTotalSize) {
        LOGI(" SHR3201 file funky length (%d)", srcLen);
        return retval;
    }
    const long* pMagic = (const long*) pHolder->GetSourceBuf(part);
    if (*pMagic != 0x00d0d0c1) {    // "APP\0"
        LOGI(" SHR3201 didn't find magic 'APP'");
        return retval;
    }
    srcBuf += 4;
    length -= 4;

    ASSERT(sizeof(fScreen.pixels) == 32000);
    ASSERT(sizeof(fExtColorTable) == 32 * 200);
    ASSERT(sizeof(fScreen.pixels) + sizeof(fExtColorTable) == kExtTotalSize);

    /* "Brooks" format color tables are stored in reverse order */
    const uint16_t* pSrcTable;
    uint16_t* pDstTable;
    pSrcTable = (const uint16_t*) srcBuf;
    pDstTable = (uint16_t*) fExtColorTable;
    int table;
    for (table = 0; table < kExtNumColorTables; table++) {
        int entry;
        for (entry = 0; entry < kNumEntriesPerColorTable; entry++) {
            pDstTable[(kNumEntriesPerColorTable-1) - entry] = *pSrcTable++;
        }
        pDstTable += kNumEntriesPerColorTable;
    }

    srcBuf = (const uint8_t*) pSrcTable;
    length -= srcBuf - (pHolder->GetSourceBuf(part) +4);

    /* now unpack the PackBytes-format pixels */
    if (UnpackBytes(fScreen.pixels, srcBuf, sizeof(fScreen.pixels), length)
            != sizeof(fScreen.pixels)) {
        goto bail;
    }

    pDib = SHR3200ToBitmap24();
    if (pDib == NULL)
        goto bail;

    SetResultBuffer(pOutput, pDib);
    retval = 0;

bail:
    delete[] tmpBuf;
    return retval;
}
