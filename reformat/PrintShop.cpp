/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Reformat Print Shop graphics.
 *
 * The "image editor" in Print Shop GS shows a nearly square image.  It looks
 * like it doubles with width and triples the height, resulting in a
 * 176x156 image.  We could do that here, but that would make it harder to
 * manipulate the basic pixels if a different aspect ratio were desired.
 * (May want to make that an option someday.)
 */
#include "StdAfx.h"
#include "PrintShop.h"

/*
 * Decide whether or not we want to handle this file.
 */
void ReformatPrintShop::Examine(ReformatHolder* pHolder)
{
    ReformatHolder::ReformatApplies applies = ReformatHolder::kApplicNot;
    long fileType = pHolder->GetFileType();
    long auxType = pHolder->GetAuxType();
    long length = pHolder->GetSourceLen(ReformatHolder::kPartData);
    bool relaxed;

    relaxed = pHolder->GetOption(ReformatHolder::kOptRelaxGfxTypeCheck) != 0;

    if (length == 576 && fileType == kTypeBIN) {
        /* PrintShop monochrome */
        if (auxType == 0x4800 || auxType == 0x5800 ||
            auxType == 0x6800 || auxType == 0x7800)
            applies = ReformatHolder::kApplicYes;
        else
            applies = ReformatHolder::kApplicMaybe; // below monitor listing
    } else if (length == 1716) {
        /* PrintShop GS color */
        if (fileType == 0xf8) {
            if (auxType == 0xc323)
                applies = ReformatHolder::kApplicYes;
            else
                applies = ReformatHolder::kApplicProbably;
        } else if (fileType == kTypeBIN && relaxed)
            applies = ReformatHolder::kApplicMaybe;
    } else if (length == 572) {
        /* PrintShop GS monochrome */
        if (fileType == 0xf8 && auxType == 0xc313)
            applies = ReformatHolder::kApplicYes;
    }

    pHolder->SetApplic(ReformatHolder::kReformatPrintShop, applies,
        ReformatHolder::kApplicNot, ReformatHolder::kApplicNot);
}

/*
 * Convert Print Shop clip art into a DIB.
 */
int ReformatPrintShop::Process(const ReformatHolder* pHolder,
    ReformatHolder::ReformatID id, ReformatHolder::ReformatPart part,
    ReformatOutput* pOutput)
{
    MyDIBitmap* pDib = NULL;
    const uint8_t* srcBuf = pHolder->GetSourceBuf(part);
    long srcLen = pHolder->GetSourceLen(part);

    if (srcLen == 572 || srcLen == 576) {
        pDib = ConvertBW(srcBuf);
    } else if (srcLen == 1716) {
        pDib = ConvertColor(srcBuf);
    } else {
        LOGI("PS shouldn't be here (len=%ld)", srcLen);
        return -1;
    }

    if (pDib == NULL) {
        LOGI("DIB creation failed");
        return -1;
    }

    SetResultBuffer(pOutput, pDib);
    return 0;
}

/*
 * Convert a B&W "classic" Print Shop graphic.
 *
 * This does a "straight" conversion without half-pixel shifting
 * or other monkey business.
 *
 * The file is a linear 1-bit 88x52 image, with four extra bytes of data
 * at the end.
 */
MyDIBitmap* ReformatPrintShop::ConvertBW(const uint8_t* srcBuf)
{
    MyDIBitmap* pDib = new MyDIBitmap;
    uint8_t* outBuf;
    uint8_t* ptr;
    int pitch;
    int x, y;

    if (pDib == NULL)
        return NULL;

    RGBQUAD colorConv[2];
    colorConv[0].rgbRed = colorConv[0].rgbGreen = colorConv[0].rgbBlue = 255;
    colorConv[1].rgbRed = colorConv[1].rgbGreen = colorConv[1].rgbBlue = 0;
    colorConv[0].rgbReserved = colorConv[1].rgbReserved = 0;

    outBuf = (uint8_t*) pDib->Create(kWidth, kHeight, 1, 2);
    if (outBuf == NULL) {
        delete pDib;
        pDib = NULL;
        goto bail;
    }
    pDib->SetColorTable(colorConv);

    pitch = pDib->GetPitch();

    /* build it in standard upside-down Windows format */
    for (y = kHeight-1; y >= 0; y--) {
        ptr = outBuf + y * pitch;
        for (x = 0; x < kWidth/8; x++)
            *ptr++ = *srcBuf++;
    }

bail:
    return pDib;
}

/*
 * The format is similar to the B&W version, but instead of one bitmap
 * there are three.  They're sorta-kinda CMY:
 *
 * >[...]                         There are 3 bit maps for the graphic. The
 * >first is yellow, the second is magenta and the third is cyan. The other
 * >colors are from combinations. Yellow and magenta is orange. Yellow and cyan
 * >is green. Magenta and cyan are purple. All three is black.
 *
 * It appears, based on running Print Shop GS in an emulator, that "cyan"
 * is actually 100% blue and "magenta" is 100% red.  The values in the color
 * table below come from a screen capture of KEGS.
 */
MyDIBitmap* ReformatPrintShop::ConvertColor(const uint8_t* srcBuf)
{
    MyDIBitmap* pDib = new MyDIBitmap;
    uint8_t* outBuf;
    uint8_t* ptr;
    uint8_t outVal;
    uint16_t yellow, magenta, cyan;
    int pitch;
    int x, y, bit;
    static const RGBQUAD kColorConv[8] = {
        /* blue, green, red, reserved  YMC */
        { 0xff, 0xff, 0xff },       // 000 white
        { 0xff, 0x00, 0x00 },       // 001 cyan (blue)
        { 0x00, 0x00, 0xff },       // 010 magenta (red)
        { 0xcc, 0x00, 0xcc },       // 011 purple
        { 0x00, 0xff, 0xff },       // 100 yellow
        { 0x00, 0xff, 0x00 },       // 101 green
        { 0x00, 0x66, 0xff },       // 110 orange
        { 0x00, 0x00, 0x00 },       // 111 black
    };

    if (pDib == NULL)
        return NULL;

    outBuf = (uint8_t*) pDib->Create(kWidth, kHeight, 4, 8);
    if (outBuf == NULL) {
        delete pDib;
        pDib = NULL;
        goto bail;
    }
    pDib->SetColorTable(kColorConv);

    pitch = pDib->GetPitch();

    /*
     * Build it in standard upside-down Windows format.
     *
     * We pre-shift the yellow/magenta/cyan values into offsetting positions
     * to save ourselves a shift each time through the loop.
     */
    for (y = kHeight-1; y >= 0; y--) {
        ptr = outBuf + y * pitch;
        for (x = 0; x < kWidth/8; x++) {
            yellow = *srcBuf << 2;
            magenta = *(srcBuf + (kWidth/8)*kHeight) << 1;
            cyan = *(srcBuf + (kWidth/8)*kHeight *2);

            /* each 3-bit combo turns into a 4-bit index, 2 per output byte */
            for (bit = 0; bit < 4; bit++) {
                outVal = ((yellow & 0x200) | (magenta & 0x100) | (cyan & 0x80)) >> 3;
                yellow <<= 1;
                magenta <<= 1;
                cyan <<= 1;
                outVal |= ((yellow & 0x200) | (magenta & 0x100) | (cyan & 0x80)) >> 7;
                yellow <<= 1;
                magenta <<= 1;
                cyan <<= 1;
                *ptr++ = outVal;
            }

            srcBuf++;
        }
    }

bail:
    return pDib;
}
