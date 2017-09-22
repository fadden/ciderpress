/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Convert the various Super HiRes formats.
 */
#ifndef REFORMAT_SUPERHIRES_H
#define REFORMAT_SUPERHIRES_H

#include "ReformatBase.h"

/*
 * Reformat SHR images (abstract base class).
 *
 * THOUGHT: add a "convert single line" function that copies to bytes or
 * RGBTRIPLEs or whatever.  Use it in the usual functions and for converting
 * odd-sized APF-format images.  Args are the bits for the line, 320/640 mode
 * for source-pixel interpretation, and the color palette for that line.  We
 * need to orient less on the format we're converting *to* (e.g. 320/640x200)
 * and more on what we're converting *from* (arbitrary sized image with
 * certain GS-specific characteristics).
 */
class ReformatSHR : public ReformatGraphics {
public:
    enum {
        kNumLines = 200,
        kPixelBytesPerLine = 160,
        kNumColorTables = 16,
        kNumEntriesPerColorTable = 16,
        kColorTableEntrySize = 2,
        kOutputWidth = 640,         // use pixel-doubling so we can get both
        kOutputHeight = 400,        //   320 and 640 modes
        kTotalSize = 32768,
        kMaxPixelsPerScan = 1280,   // allow up to 1280x1024
        kMaxScanLines = 1024,       //  in APF images

        kSCBColorTableMask = 0x0f,
        kSCBFillMode = 0x20,
        kSCBInterrupts = 0x40,
        kSCBNumPixels = 0x80,       // 0=320, 1=640
    };

    /*
     * This holds one SHR screen; the size must be 32768 bytes.
     */
    typedef struct SHRScreen {
        uint8_t pixels[kNumLines * kPixelBytesPerLine];
        uint8_t scb[kNumLines];
        uint8_t reserved[256 - kNumLines];
        uint8_t colorTable[kNumColorTables * kNumEntriesPerColorTable *
                                    kColorTableEntrySize];
    } SHRScreen;

    /* convert 0RGB into (R,G,B) */
    void GSColor(int color, RGBTRIPLE* pTriple) {
        pTriple->rgbtRed = ((color >> 8) & 0x0f) * 17;
        pTriple->rgbtGreen = ((color >> 4) & 0x0f) * 17;
        pTriple->rgbtBlue = (color & 0x0f) * 17;
    }
    void GSColor(int color, RGBQUAD* pQuad) {
        pQuad->rgbRed = ((color >> 8) & 0x0f) * 17;
        pQuad->rgbGreen = ((color >> 4) & 0x0f) * 17;
        pQuad->rgbBlue = (color & 0x0f) * 17;
        pQuad->rgbReserved = 0;
    }

    /* 16 palettes of 16 colors */
    RGBQUAD     fColorTables[kNumColorTables][kNumEntriesPerColorTable];

    /*
     * Convert a SHRScreen struct to a 256-color DIB.
     *
     * This is a reasonably generic routine shared by SHR functions.
     */
    MyDIBitmap* SHRScreenToBitmap8(const SHRScreen* pScreen);

    /*
     * Convert tables of SHR data to an 8bpp bitmap.  The expectation is
     * that we double-up 320-mode pixels and double the height, so the
     * output for a standard SHR image will be a 640x400 bitmap.
     *
     * "pPixels" is the 2-bit or 4-bit pixel data.
     * "pSCB" points to an array of SCB values, one per scan line.
     * "pColorTable" points to an array of 16 color tables (16 entries each).
     * "bytesPerLine" is the number of whole bytes per line.
     * "numScanLines" is the number of scan lines.
     * "outputWidthPix" is the width, in pixels (640 for full screen).
     * "outputHeightPix" is the height, in pixels (400 for full screen).
     *
     * If the source material has an odd width, part of the last byte will
     * be empty.
     */
    MyDIBitmap* SHRDataToBitmap8(const uint8_t* pPixels,
        const uint8_t* pSCB, const uint8_t* pColorTable,
        unsigned int bytesPerLine, unsigned int numScanLines,
        unsigned int outputWidthPix, unsigned int outputHeightPix);
};

/*
 * Reformat an unpacked SHR graphic into a bitmap.
 */
class ReformatUnpackedSHR : public ReformatSHR {
public:
    ReformatUnpackedSHR(void) {}
    virtual ~ReformatUnpackedSHR(void) {}

    virtual void Examine(ReformatHolder* pHolder) override;
    virtual int Process(const ReformatHolder* pHolder,
        ReformatHolder::ReformatID id, ReformatHolder::ReformatPart part,
        ReformatOutput* pOutput) override;

private:
    SHRScreen   fScreen;
};

/*
 * Reformat an unpacked ".APP" SHR graphic from John Elway Quarterback GS
 * into a bitmap.  (Implementing this was trivial if slightly silly.)
 */
class ReformatJEQSHR : public ReformatSHR {
public:
    ReformatJEQSHR(void) {}
    virtual ~ReformatJEQSHR(void) {}

    virtual void Examine(ReformatHolder* pHolder) override;
    virtual int Process(const ReformatHolder* pHolder,
        ReformatHolder::ReformatID id, ReformatHolder::ReformatPart part,
        ReformatOutput* pOutput) override;

private:
    enum { kExpectedLen = 32288 };
    SHRScreen   fScreen;
};


/*
 * Reformat a Paintworks SHR graphic ($c0/0000) into a bitmap.
 */
class ReformatPaintworksSHR : public ReformatSHR {
public:
    ReformatPaintworksSHR(void) {}
    virtual ~ReformatPaintworksSHR(void) {}

    virtual void Examine(ReformatHolder* pHolder) override;
    virtual int Process(const ReformatHolder* pHolder,
        ReformatHolder::ReformatID id, ReformatHolder::ReformatPart part,
        ReformatOutput* pOutput) override;

private:
    enum { kMinSize = 223 };
    //SHRScreen fScreen;
};


/*
 * Reformat a packed SHR graphic ($c0/0001) into a bitmap.
 */
class ReformatPackedSHR : public ReformatSHR {
public:
    ReformatPackedSHR(void) {}
    virtual ~ReformatPackedSHR(void) {}

    virtual void Examine(ReformatHolder* pHolder) override;
    virtual int Process(const ReformatHolder* pHolder,
        ReformatHolder::ReformatID id, ReformatHolder::ReformatPart part,
        ReformatOutput* pOutput) override;

private:
    SHRScreen   fScreen;
};

/*
 * Reformat an Apple-Preferred Format SHR graphic ($c0/0002) into a bitmap.
 *
 * The graphic could be a standard SHR image, a larger or smaller than normal
 * SHR image, or a 3200-color image.
 */
class ReformatAPFSHR : public ReformatSHR {
public:
    ReformatAPFSHR(void) : fNonStandard(false), fPixelStore(NULL),
        fSCBStore(NULL) {}
    virtual ~ReformatAPFSHR(void) {
        if (fPixelStore != NULL && fPixelStore != fScreen.pixels)
            delete[] fPixelStore;
        if (fSCBStore != NULL && fSCBStore != fScreen.scb)
            delete[] fSCBStore;
    }

    virtual void Examine(ReformatHolder* pHolder) override;
    virtual int Process(const ReformatHolder* pHolder,
        ReformatHolder::ReformatID id, ReformatHolder::ReformatPart part,
        ReformatOutput* pOutput) override;

private:
    int UnpackMain(const uint8_t* srcPtr, long srcLen);
    int UnpackMultipal(uint8_t* dstPtr, const uint8_t* srcPtr, long srcLen);
    void UnpackNote(const uint8_t* srcPtr, long srcLen);

    /* use this for standard-sized images */
    SHRScreen       fScreen;

    /* set for non-standard images */
    bool            fNonStandard;

    /* use this for non-standard-sized images */
    uint8_t*        fPixelStore;
    uint8_t*        fSCBStore;
    unsigned int    fNumScanLines;      // #of scan lines in image
    unsigned int    fPixelsPerScanLine;
    unsigned int    fPixelBytesPerLine;
    unsigned int    fOutputWidth;
    unsigned int    fOutputHeight;      // scan lines * 2
};

/*
 * Reformat a 3200-color SHR graphic into a bitmap.
 */
class Reformat3200SHR : public ReformatSHR {
public:
    Reformat3200SHR(void) {}
    virtual ~Reformat3200SHR(void) {}

    // alternate construction, used by APFSHR
    Reformat3200SHR(SHRScreen* pScreen, uint8_t* multiPal) {
        memcpy(&fScreen, pScreen, sizeof(fScreen));
        memcpy(fExtColorTable, multiPal, sizeof(fExtColorTable));
    }

    virtual void Examine(ReformatHolder* pHolder) override;
    virtual int Process(const ReformatHolder* pHolder,
        ReformatHolder::ReformatID id, ReformatHolder::ReformatPart part,
        ReformatOutput* pOutput) override;

    MyDIBitmap* SHR3200ToBitmap24(void);

protected:
    enum {
        kExtNumColorTables = kNumLines,
        kExtTotalSize = 38400,
    };

    SHRScreen   fScreen;        // only "pixels" is valid

    /* this holds the 200 color tables, with the entries switched to normal */
    uint8_t     fExtColorTable[kExtNumColorTables *
                            kNumEntriesPerColorTable * kColorTableEntrySize];
};

/*
 * Reformat a packed 3200-color SHR graphic.
 */
class Reformat3201SHR : public Reformat3200SHR {
public:
    Reformat3201SHR(void) {}
    virtual ~Reformat3201SHR(void) {}

    virtual void Examine(ReformatHolder* pHolder) override;
    virtual int Process(const ReformatHolder* pHolder,
        ReformatHolder::ReformatID id, ReformatHolder::ReformatPart part,
        ReformatOutput* pOutput) override;
};


/*
 * Generic DreamGrafix code.
 */
class DreamGrafix {
public:
    DreamGrafix(void) : fWidth(0), fHeight(0), fNumColors(0) {}
    ~DreamGrafix(void) {}

    /*
     * Scan the file.  If it's a DreamGrafix image, extract values from the
     * header fields and return "true".
     */
    bool ScanDreamGrafix(ReformatHolder* pHolder);

    /*
     * Unpack a DreamGrafix SHR image compressed with LZW.
     */
    bool UnpackDG(const uint8_t* srcBuf, long srcLen,
        ReformatSHR::SHRScreen* pScreen, uint8_t* extColorTable);

    int fWidth;
    int fHeight;
    int fNumColors;

private:
    static int UnpackLZW(const uint8_t* srcBuf, long srcLen,
        uint8_t* dstBuf, long dstLen);
    enum { kHeaderOffset = 17 };
};

/*
 * Reformat a DreamGrafix 256-color SHR graphic.
 */
class ReformatDG256SHR : public ReformatSHR {
public:
    ReformatDG256SHR(void) {}
    virtual ~ReformatDG256SHR(void) {}

    virtual void Examine(ReformatHolder* pHolder) override;
    virtual int Process(const ReformatHolder* pHolder,
        ReformatHolder::ReformatID id, ReformatHolder::ReformatPart part,
        ReformatOutput* pOutput) override;

private:
    DreamGrafix fDG;
    SHRScreen   fScreen;
};

/*
 * Reformat a DreamGrafix 3200-color SHR graphic.
 */
class ReformatDG3200SHR : public Reformat3200SHR {
public:
    ReformatDG3200SHR(void) {}
    virtual ~ReformatDG3200SHR(void) {}

    virtual void Examine(ReformatHolder* pHolder) override;
    virtual int Process(const ReformatHolder* pHolder,
        ReformatHolder::ReformatID id, ReformatHolder::ReformatPart part,
        ReformatOutput* pOutput) override;

private:
    DreamGrafix fDG;
};

#endif /*REFORMAT_SUPERHIRES_H*/
