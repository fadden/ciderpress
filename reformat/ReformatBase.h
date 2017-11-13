/*
 * CiderPress
 * Copyright (C) 2007, 2008 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Reformatter base classes.  The main app does not need this header file.
 *
 * Every converter turns the source into text or graphics.  Currently it's
 * not possible to convert something into a mix of both.  We could change
 * that, but we'd have to figure out what that means when extracting a file
 * (i.e. figure out the RTF embedded bitmap format).
 */
#ifndef REFORMAT_REFORMATBASE_H
#define REFORMAT_REFORMATBASE_H

#include "Reformat.h"
#include "Charset.h"

#define BufPrintf fExpBuf.Printf


/*
 * Abstract base class for reformatting a file into readable text.
 *
 * The transmuted version is written on top of the original, or is allocated
 * in new[]ed storage and replaces the original (which is delete[]d).
 */
class Reformat {
public:
    Reformat(void) {}
    virtual ~Reformat(void) {}

    enum {
        kTypePCD    = 0x02,
        kTypePTX    = 0x03,
        kTypeTXT    = 0x04,
        kTypeBIN    = 0x06,
        kTypeFOT    = 0x08,
        kTypeBA3    = 0x09,
        kTypeDIR    = 0x0f,
        kTypeADB    = 0x19,
        kTypeAWP    = 0x1a,
        kTypeASP    = 0x1b,
        kType8OB    = 0x2b,
        kTypeP8C    = 0x2e,
        kTypeGWP    = 0x50,
        kTypeOBJ    = 0xb1,
        kTypeLIB    = 0xb2,
        kTypeFST    = 0xbd,
        kTypePNT    = 0xc0,
        kTypePIC    = 0xc1,
        kTypeCMD    = 0xf0,
        kTypeDOS_B  = 0xf4,     // alternate 'B'
        kTypeOS     = 0xf9,
        kTypeINT    = 0xfa,
        kTypeBAS    = 0xfc,
        kTypeSYS    = 0xff,
    };

    /* test applicability of all file parts */
    virtual void Examine(ReformatHolder* pHolder) = 0;

    /* reformat appropriately; returns 0 on success, -1 on error */
    virtual int Process(const ReformatHolder* pHolder,
        ReformatHolder::ReformatID id, ReformatHolder::ReformatPart part,
        ReformatOutput* pOutput) = 0;

    // grab the next 8 bits
    static inline uint8_t Read8(const uint8_t** pBuf, long* pLength) {
        if (*pLength > 0) {
            (*pLength)--;
            return *(*pBuf)++;
        } else {
            // ought to throw an exception here
            ASSERT(false);
            return (uint8_t) -1;
        }
    }
    // grab a 16-bit little-endian value
    static inline uint16_t Read16(const uint8_t** pBuf, long* pLength) {
        uint16_t val;
        if (*pLength >= 2) {
            val = *(*pBuf)++;
            val |= *(*pBuf)++ << 8;
            *pLength -= 2;
        } else {
            // ought to throw an exception here
            ASSERT(false);
            val = (uint16_t) -1;
        }
        return val;
    }
    // grab a 32-bit little-endian value
    static inline uint32_t Read32(const uint8_t** pBuf, long* pLength) {
        uint32_t val;
        if (*pLength >= 4) {
            val = *(*pBuf)++;
            val |= *(*pBuf)++ << 8;
            val |= *(*pBuf)++ << 16;
            val |= *(*pBuf)++ << 24;
            *pLength -= 4;
        } else {
            // ought to throw an exception here
            ASSERT(false);
            val = (uint32_t) -1;
        }
        return val;
    }

    static inline uint16_t Get16LE(const uint8_t* buf) {
        return *buf | *(buf+1) << 8;
    }
    static inline uint32_t Get32LE(const uint8_t* buf) {
        return *buf | *(buf+1) << 8 | *(buf+2) << 16 | *(buf+3) << 24;
    }
    static inline uint16_t Get16BE(const uint8_t* buf) {
        return *buf << 8 | *(buf+1);
    }
    static inline uint32_t Get32BE(const uint8_t* buf) {
        return *buf << 24 | *(buf+1) << 16 | *(buf+2) << 8 | *(buf+3);
    }
    static inline uint16_t Get16(const uint8_t* buf, bool littleEndian) {
        if (littleEndian)
            return Get16LE(buf);
        else
            return Get16BE(buf);
    }
    static inline uint32_t Get32(const uint8_t* buf, bool littleEndian) {
        if (littleEndian)
            return Get32LE(buf);
        else
            return Get32BE(buf);
    }

private:
    DECLARE_COPY_AND_OPEQ(Reformat)
};

/*
 * Abstract base class for reformatting a graphics file into a
 * device-independent bitmap..
 */
class ReformatGraphics: public Reformat {
public:
    ReformatGraphics() {
        InitPalette();
    }

protected:
    void SetResultBuffer(ReformatOutput* pOutput, MyDIBitmap* pDib);

    /*
     * Color palette to use for color conversions.  We store it here
     * so it can be configured to suit the user's tastes.
     */
    enum { kPaletteBlack, kPaletteRed, kPaletteDarkBlue,
           kPalettePurple, kPaletteDarkGreen, kPaletteDarkGrey,
           kPaletteMediumBlue, kPaletteLightBlue, kPaletteBrown,
           kPaletteOrange, kPaletteLightGrey, kPalettePink,
           kPaletteGreen, kPaletteYellow, kPaletteAqua,
           kPaletteWhite, kPaletteSize };
    RGBQUAD fPalette[kPaletteSize];

    int UnpackBytes(uint8_t* dst, const uint8_t* src,
        long dstRem, long srcLen);
    void UnPackBits(const uint8_t** pSrcBuf, long* pSrcLen,
        uint8_t** pOutPtr, long dstLen, uint8_t xorVal);

private:
    DECLARE_COPY_AND_OPEQ(ReformatGraphics)

    void InitPalette();
};

/*
 * Abstract base class for reformatting a file into readable text.
 *
 * Includes an expanding buffer that can be appended to, and a set of RTF
 * primitives for adding structure.
 */
class ReformatText : public Reformat {
public:
    enum ParagraphJustify {
        kJustifyLeft,
        kJustifyRight,
        kJustifyCenter,
        kJustifyFull,
    };

    ReformatText()
      : fUseRTF(true),
        fLeftMargin(0),
        fRightMargin(0),
        fPointSize(8),
        fPreMultPointSize(8),
        fGSFontSizeMult(1.0),
        fBoldEnabled(false),
        fItalicEnabled(false),
        fUnderlineEnabled(false),
        fInverseEnabled(false),
        fOutlineEnabled(false),
        fShadowEnabled(false),
        fSuperscriptEnabled(false),
        fSubscriptEnabled(false),
        fJustified(kJustifyLeft),
        fTextColor(kColorNone)
    {}
    virtual ~ReformatText(void) {}

    /*
     * The numeric values are determined by the RTF header that we output.
     * If the header in RTFBegin() changes, update these values.
     */
    typedef enum RTFFont {
        // basic fonts (one monospace, one proportional, Courier New default)
        kFontCourierNew     = 0,
        kFontTimesRoman     = 1,
        kFontArial          = 2,
        kFontSymbol         = 3,
    } RTFFont;
    typedef enum {
        kColorNone          = 0,
        // full colors (RGB 0 or 255)
        kColorBlack         = 1,
        kColorBlue          = 2,
        kColorCyan          = 3,
        kColorGreen         = 4,        // a little bright for white bkgnd
        kColorPink          = 5,
        kColorRed           = 6,
        kColorYellow        = 7,
        kColorWhite         = 8,
        // mixed colors
        kColorMediumBlue    = 9,
        kColorMediumAqua    = 10,
        kColorMediumGreen   = 11,
        kColorMagena        = 12,
        kColorMediumRed     = 13,
        kColorOlive         = 14,
        kColorMediumGrey    = 15,
        kColorLightGrey     = 16,
        kColorDarkGrey      = 17,
        kColorOrange        = 18,
    } TextColor;

    /* Apple IIgs families */
    typedef enum GSFontFamily {
        // standard fonts, defined in toolbox ref
        kGSFontNewYork      = 0x0002,
        kGSFontGeneva       = 0x0003,   // sans-sarif, like Arial
        kGSFontMonaco       = 0x0004,   // monospace
        kGSFontVenice       = 0x0005,   // script font
        kGSFontLondon       = 0x0006,
        kGSFontAthens       = 0x0007,
        kGSFontSanFran      = 0x0008,
        kGSFontToronto      = 0x0009,
        kGSFontCairo        = 0x000b,
        kGSFontLosAngeles   = 0x000c,
        kGSFontTimes        = 0x0014,   // sarif, equal to Times New Roman
        kGSFontHelvetica    = 0x0015,   // sans-sarif, equal to Arial
        kGSFontCourier      = 0x0016,   // monospace, sarif, Courier
        kGSFontSymbol       = 0x0017,
        kGSFontTaliesin     = 0x0018,
        // I had these installed, by apps or by Pointless
        kGSFontStarfleet    = 0x078d,
        kGSFontWestern      = 0x088e,
        kGSFontGenoa        = 0x0bcb,
        kGSFontClassical    = 0x2baa,
        kGSFontChicago      = 0x3fff,
        kGSFontGenesys      = 0x7530,
        kGSFontPCMonospace  = 0x7fdc,   // monospace, sans-sarif
        kGSFontAppleM       = 0x7f58,   // looks like classic Apple II font
        kGSFontUnknown1     = 0x9c50,   // found in French AWGS doc "CONVSEC"
        kGSFontUnknown2     = 0x9c54,   // found in French AWGS doc "CONVSEC"
        // ROM font
        kGSFontShaston      = 0xfffe,   // rounded sans-sarif
    } GSFontFamily;

    /* QuickDraw II font styles; this is a bit mask */
    typedef enum QDFontStyle {
        kQDStyleBold            = 0x01,
        kQDStyleItalic          = 0x02,
        kQDStyleUnderline       = 0x04,
        kQDStyleOutline         = 0x08,
        kQDStyleShadow          = 0x10,
        kQDStyleReserved        = 0x20,
        kQDStyleSuperscript     = 0x40,     // not in QDII -- AWGS only
        kQDStyleSubscript       = 0x80,     // not in QDII -- AWGS only
    } QDFontStyle;

    /* flags for RTFBegin */
    enum {
        kRTFFlagColorTable  = 1,        // include color table
    };

protected:
    void RTFBegin(int flags = 0);
    void RTFEnd(void);
    void RTFSetPara(void);
    void RTFNewPara(void);
    void RTFPageBreak(void);
    void RTFTab(void);
    void RTFBoldOn(void);
    void RTFBoldOff(void);
    void RTFItalicOn(void);
    void RTFItalicOff(void);
    void RTFUnderlineOn(void);
    void RTFUnderlineOff(void);
    void RTFInverseOn(void);
    void RTFInverseOff(void);
    void RTFParaLeft(void);
    void RTFParaRight(void);
    void RTFParaCenter(void);
    void RTFParaJustify(void);
    void RTFSubscriptOn(void);
    void RTFSubscriptOff(void);
    void RTFSuperscriptOn(void);
    void RTFSuperscriptOff(void);
    void RTFOutlineOn(void);
    void RTFOutlineOff(void);
    void RTFShadowOn(void);
    void RTFShadowOff(void);
    TextColor RTFSetColor(TextColor color);
    void RTFSetFont(RTFFont font);
    void RTFSetFontSize(int points);
    void RTFLeftMargin(int margin);
    void RTFRightMargin(int margin);
    void RTFSetGSFont(uint16_t family);
    void RTFSetGSFontSize(int points);
    void RTFSetGSFontStyle(uint8_t qdStyle);
//  void RTFProportionalOn(void);
//  void RTFProportionalOff(void);

    void ConvertEOL(const uint8_t* srcBuf, long srcLen,
        bool stripHiBits);
    void ConvertEOL(const uint8_t* srcBuf, long srcLen,
        bool stripHiBits, bool stripNulls);
    void BufHexDump(const uint8_t* srcBuf, long srcLen);
    void SetResultBuffer(ReformatOutput* pOutput, bool multiFont = false);

    ExpandBuffer    fExpBuf;
    bool            fUseRTF;

    // return a low-ASCII character so we can read high-ASCII files
    inline char PrintableChar(uint8_t ch) {
        if (ch < 0x20)
            return '.';
        else if (ch < 0x7f)
            return ch;
        else if (ch < 0xa0 || ch == 0xff)   // 0xff becomes 0x7f
            return '.';
        else
            return ch & 0x7f;
    }
    // output an RTF-escaped char (do we want to trap Ctrl-Z?)
    // (only use this if we're in RTF mode)
    inline void RTFPrintChar(uint8_t ch) {
        char pch = PrintableChar(ch);
        RTFPrintUTF16Char(pch);
    }
    // output an RTF-escaped char, allowing UTF-16 Unicode values
    // (only use this if we're in RTF mode)
    inline void RTFPrintUTF16Char(uint16_t ch) {
        if (ch == '\\') {
            fExpBuf.Printf("\\\\");
        } else if (ch == '{') {
            fExpBuf.Printf("\\{");
        } else if (ch == '}') {
            fExpBuf.Printf("\\}");
        } else if (ch >= 0x20 && ch < 0x80) {
            // don't use Unicode escapes for these, or the output will be
            // unreadable by mere humans
            fExpBuf.Printf("%c", ch);
        } else {
            // must print as a *signed* 16-bit decimal value, though it
            // looks like most parsers work either way
            fExpBuf.Printf("\\u%d?", (int16_t)ch);
        }
    }
    // output a char, doubling up double quotes (for .CSV)
    inline void BufPrintQChar(uint8_t ch) {
        if (ch == '"')
            fExpBuf.Printf("\"\"");
        else
            fExpBuf.Printf("%c", ch);
    }

    // Converts a MouseText value (0-31) to a 16-bit UTF-16 value.  If
    // the character is in the BMP then only low is used.  If it requires
    // encoding as a surrogate pair, high will be nonzero.
    void MouseTextToUTF16(uint8_t mtVal, uint16_t* pLow, uint16_t* pHigh);

    // Converts a MouseText value (0-31) to an ASCII equivalent.  This is not
    // a 1:1 conversion, so it's not reversible.
    int8_t MouseTextToASCII(uint8_t mtVal);

private:
    DECLARE_COPY_AND_OPEQ(ReformatText)
    int CreateWorkBuf(void);

    enum { kRTFUnitsPerInch = 1440 };   // TWIPS

    int     fLeftMargin, fRightMargin;  // for documents, in 1/10th inch
    int     fPointSize;
    int     fPreMultPointSize;
    float   fGSFontSizeMult;
    bool    fBoldEnabled;
    bool    fItalicEnabled;
    bool    fUnderlineEnabled;
    bool    fInverseEnabled;
    bool    fOutlineEnabled;
    bool    fShadowEnabled;
    bool    fSuperscriptEnabled;
    bool    fSubscriptEnabled;
    ParagraphJustify    fJustified;
    TextColor   fTextColor;
};

#endif /*REFORMAT_REFORMATBASE_H*/
