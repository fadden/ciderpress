/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Reformatter base class implementation.
 */
#include "StdAfx.h"
#include "ReformatBase.h"
#include <math.h>


/*
 * ==========================================================================
 *      ReformatText
 * ==========================================================================
 */

/*
 * Set the output format and buffer.
 *
 * Clears our work buffer pointer so we don't free it.
 */
void ReformatText::SetResultBuffer(ReformatOutput* pOutput, bool multiFont)
{
    char* buf;
    long len;
    fExpBuf.SeizeBuffer(&buf, &len);
    pOutput->SetTextBuf(buf, len, true);

    if (pOutput->GetTextBuf() == NULL) {
        /*
         * Force "raw" mode if there's no output.  This can happen if we,
         * say, try to format an empty file as a hex dump.  We never
         * produce any output, so no buffer gets allocated.
         *
         * We set the mode to "raw" so that applications can assume that
         * results of type "text" actually have text to look at -- though
         * it's possible the length will be zero, we promise that there'll
         * be a buffer there.  I'm not sure it's important to do this,
         * but it does reduce the #of situations in which we have to
         * worry about NULL pointers.
         */
        pOutput->SetOutputKind(ReformatOutput::kOutputRaw);
        LOGI("ReformatText returning a null pointer");
    } else {
        if (fUseRTF)
            pOutput->SetOutputKind(ReformatOutput::kOutputRTF);
        else
            pOutput->SetOutputKind(ReformatOutput::kOutputText);
    }

    if (fUseRTF && multiFont)
        pOutput->SetMultipleFontsFlag(true);
}

/*
 * Output the RTF header.
 *
 * The color table is the standard MS Word color table, except that entry
 * #17 (dark grey) has been lightened from (51,51,51) because it's nearly
 * indistinguishable from black on the screen.
 *
 * The default font is Courier New (\f0) at 10 points (\fs20).
 */
void ReformatText::RTFBegin(int flags)
{
//  static const char* rtfHdr =
//"{\\rtf1\\ansi\\ansicpg1252\\deff0\\deflang1033{\\fonttbl{\\f0\\fmodern\\fprq1\\fcharset0 Courier New;}}\r\n"
//"\\viewkind4\\uc1\\pard\\f0\\fs20 ";

    static const char* rtfHdrStart =
"{\\rtf1\\ansi\\ansicpg1252\\deff0\\deflang1033\\deflangfe1033{\\fonttbl"
    "{\\f0\\fmodern\\fprq1\\fcharset0 Courier New;}"
    "{\\f1\\froman\\fprq2\\fcharset0 Times New Roman;}"
    "{\\f2\\fswiss\\fprq2\\fcharset0 Arial;}"
    "{\\f3\\froman\\fprq2\\fcharset2 Symbol;}"
    "}\r\n";

    static const char* rtfColorTable = 
"{\\colortbl;"
    "\\red0\\green0\\blue0;\\red0\\green0\\blue255;\\red0\\green255\\blue255;\\red0\\green255\\blue0;"
    "\\red255\\green0\\blue255;\\red255\\green0\\blue0;\\red255\\green255\\blue0;\\red255\\green255\\blue255;"
    "\\red0\\green0\\blue128;\\red0\\green128\\blue128;\\red0\\green128\\blue0;\r\n"
    "\\red128\\green0\\blue128;\\red128\\green0\\blue0;\\red128\\green128\\blue0;\\red128\\green128\\blue128;"
    "\\red192\\green192\\blue192;\\red64\\green64\\blue64;\\red255\\green153\\blue0;}\r\n";

    static const char* rtfHdrEnd =
"\\viewkind4\\uc1\\pard\\f0\\fs20 ";

    if (fUseRTF) {
        BufPrintf("%s", rtfHdrStart);
        if ((flags & kRTFFlagColorTable) != 0)
            BufPrintf("%s", rtfColorTable);
        BufPrintf("%s", rtfHdrEnd);
    }

    fPointSize = 10;
}

/*
 * Output the RTF footer.
 */
void ReformatText::RTFEnd(void)
{
    if (fUseRTF) BufPrintf("}\r\n%c", '\0');
}

/*
 * Output RTF paragraph definition marker.  Do this every time we change some
 * aspect of paragraph formatting, such as margins or justification.
 */
void ReformatText::RTFSetPara(void)
{
    if (!fUseRTF)
        return;

    BufPrintf("\\pard\\nowidctlpar");

    if (fLeftMargin != 0 || fRightMargin != 0) {
        /* looks like RTF thinks we're getting 12 chars per inch? */
        if (fLeftMargin != 0)
            BufPrintf("\\li%d",
                (int) (fLeftMargin * (kRTFUnitsPerInch/12)));
        if (fLeftMargin != 0)
            BufPrintf("\\ri%d",
                (int) (fRightMargin * (kRTFUnitsPerInch/12)));
    }

    switch (fJustified) {
    case kJustifyLeft:                          break;
    case kJustifyRight:     BufPrintf("\\qr");  break;
    case kJustifyCenter:    BufPrintf("\\qc");  break;
    case kJustifyFull:      BufPrintf("\\qj");  break;
    default:
        assert(false);
        break;
    }

    // Ideally we'd suppress this if the next thing is an RTF
    //  formatting command, esp. "\\par".
    BufPrintf(" ");
}

/*
 * Output a new paragraph marker.
 *
 * If you're producing RTF output, this is the right way to output an
 * end-of-line character.
 */
void ReformatText::RTFNewPara(void)
{
    if (fUseRTF)
        BufPrintf("\\par\r\n");
    else
        BufPrintf("\r\n");
}


/*
 * Insert a page break.  This isn't supported by the Rich Edit control,
 * so it won't appear in CiderPress or WordPad, but it will come out in
 * Microsoft Word if you extract to a file.
 */
void ReformatText::RTFPageBreak(void)
{
    if (fUseRTF)
        BufPrintf("\\page ");
}

/*
 * RTF tab character.
 */
void ReformatText::RTFTab(void)
{
    if (fUseRTF)
        BufPrintf("\\tab ");
}

/*
 * Minor formatting.
 */
void ReformatText::RTFBoldOn(void)
{
    if (fBoldEnabled)
        return;
    if (fUseRTF) {
        BufPrintf("\\b ");
        fBoldEnabled = true;
    }
}

void ReformatText::RTFBoldOff(void)
{
    if (!fBoldEnabled)
        return;
    if (fUseRTF) {
        BufPrintf("\\b0 ");
        fBoldEnabled = false;
    }
}

void ReformatText::RTFItalicOn(void)
{
    if (fItalicEnabled)
        return;
    if (fUseRTF) {
        BufPrintf("\\i ");
        fItalicEnabled = true;
    }
}

void ReformatText::RTFItalicOff(void)
{
    if (!fItalicEnabled)
        return;
    if (fUseRTF) {
        BufPrintf("\\i0 ");
        fItalicEnabled = false;
    }
}

void ReformatText::RTFUnderlineOn(void)
{
    if (fUnderlineEnabled)
        return;
    if (fUseRTF) {
        BufPrintf("\\ul ");
        fUnderlineEnabled = true;
    }
}

void ReformatText::RTFUnderlineOff(void)
{
    if (!fUnderlineEnabled)
        return;
    if (fUseRTF) {
        BufPrintf("\\ulnone ");
        fUnderlineEnabled = false;
    }
}

void ReformatText::RTFInverseOn(void)
{
    if (fInverseEnabled)
        return;
    if (fUseRTF) {
        RTFSetColor(TextColor::kColorWhite);
        BufPrintf("\\highlight1 ");     // black
        fInverseEnabled = true;
    }
}

void ReformatText::RTFInverseOff(void)
{
    if (!fInverseEnabled)
        return;
    if (fUseRTF) {
        RTFSetColor(TextColor::kColorNone);
        BufPrintf("\\highlight0 ");
        fInverseEnabled = false;
    }
}

void ReformatText::RTFOutlineOn(void)
{
    if (fOutlineEnabled)
        return;
    if (fUseRTF) {
        BufPrintf("\\outl ");
        fOutlineEnabled = true;
    }
}

void ReformatText::RTFOutlineOff(void)
{
    if (!fOutlineEnabled)
        return;
    if (fUseRTF) {
        BufPrintf("\\outl0 ");
        fOutlineEnabled = false;
    }
}

void ReformatText::RTFShadowOn(void)
{
    if (fShadowEnabled)
        return;
    if (fUseRTF) {
        BufPrintf("\\shad ");
        fShadowEnabled = true;
    }
}

void ReformatText::RTFShadowOff(void)
{
    if (!fShadowEnabled)
        return;
    if (fUseRTF) {
        BufPrintf("\\shad0 ");
        fShadowEnabled = false;
    }
}

void ReformatText::RTFSubscriptOn(void)
{
    if (fSubscriptEnabled)
        return;
    if (fUseRTF) {
        BufPrintf("\\sub ");
        fSubscriptEnabled = true;
    }
}

void ReformatText::RTFSubscriptOff(void)
{
    if (!fSubscriptEnabled)
        return;
    if (fUseRTF) {
        BufPrintf("\\nosupersub ");
        fSubscriptEnabled = false;
    }
}

void ReformatText::RTFSuperscriptOn(void)
{
    if (fSuperscriptEnabled)
        return;
    if (fUseRTF) {
        BufPrintf("\\super ");
        fSuperscriptEnabled = true;
    }
}

void ReformatText::RTFSuperscriptOff(void)
{
    if (!fSuperscriptEnabled)
        return;
    if (fUseRTF) {
        BufPrintf("\\nosupersub ");
        fSuperscriptEnabled = false;
    }
}

ReformatText::TextColor ReformatText::RTFSetColor(TextColor color)
{
    TextColor oldColor = fTextColor;
    if (color != fTextColor && fUseRTF) {
        BufPrintf("\\cf%d ", color);
        fTextColor = color;
    }
    return oldColor;
}

/*
 * Change paragraph formatting.
 */
void ReformatText::RTFParaLeft(void)
{
    if (fJustified != kJustifyLeft) {
        fJustified = kJustifyLeft;
        RTFSetPara();
    }
}

void ReformatText::RTFParaRight(void)
{
    if (fJustified != kJustifyRight) {
        fJustified = kJustifyRight;
        RTFSetPara();
    }
}

void ReformatText::RTFParaCenter(void)
{
    if (fJustified != kJustifyCenter) {
        fJustified = kJustifyCenter;
        RTFSetPara();
    }
}

void ReformatText::RTFParaJustify(void)
{
    if (fJustified != kJustifyFull) {
        fJustified = kJustifyFull;
        RTFSetPara();
    }
}

/*
 * Page margins, in 1/10th inches.
 */
void ReformatText::RTFLeftMargin(int margin)
{
    //LOGI("+++ Left margin now %d", margin);
    fLeftMargin = margin;
    RTFSetPara();
}

void ReformatText::RTFRightMargin(int margin)
{
    //LOGI("+++ Right margin now %d", margin);
    fRightMargin = margin;
    RTFSetPara();
}

/*
 * Switch to a different font size.
 */
void ReformatText::RTFSetFontSize(int points)
{
    if (fUseRTF && fPointSize != points)
        BufPrintf("\\fs%d ", points * 2);
    fPointSize = points;
}
/*
 * Switch to a different font.
 */
void ReformatText::RTFSetFont(RTFFont font)
{
    if (fUseRTF)
        BufPrintf("\\f%d ", font);
}

/*
 * Set the font by specifying a IIgs QuickDraw II font family number.
 */
void ReformatText::RTFSetGSFont(uint16_t family)
{
    float newMult;

    if (!fUseRTF)
        return;

    /*
     * Apple II fonts seem to be about 1.5x in a WYSIWYG way, except
     * for Times, which is about 1:1.
     */
    switch (family) {
    case kGSFontTimes:
        RTFSetFont(kFontTimesRoman);
        newMult = 0.9f;
        break;
    case kGSFontNewYork:
        RTFSetFont(kFontTimesRoman);
        newMult = 1.1f;
        break;

    case kGSFontSymbol:
        RTFSetFont(kFontSymbol);
        newMult = 1.0f;
        break;

    case kGSFontMonaco:
        RTFSetFont(kFontCourierNew);
        newMult = 0.80f;
        break;
    case kGSFontCourier:
    case kGSFontPCMonospace:
    case kGSFontAppleM:
    case kGSFontGenesys:
        RTFSetFont(kFontCourierNew);
        newMult = 1.5f;
        break;

    case kGSFontClassical:
    case kGSFontGenoa:
    case kGSFontWestern:
        RTFSetFont(kFontArial);
        newMult = 0.80f;
        break;
    case kGSFontChicago:
    case kGSFontVenice:
    case kGSFontGeneva:
    case kGSFontStarfleet:
    case kGSFontUnknown1:
    case kGSFontUnknown2:
        RTFSetFont(kFontArial);
        newMult = 1.0f;
        break;
    case kGSFontLondon:
    case kGSFontAthens:
    case kGSFontSanFran:
    case kGSFontShaston:
    case kGSFontToronto:
    case kGSFontCairo:
    case kGSFontLosAngeles:
    case kGSFontHelvetica:
    case kGSFontTaliesin:
        RTFSetFont(kFontArial);
        newMult = 1.5f;
        break;
    default:
        LOGI("Unrecognized font family 0x%04x, using Arial", family);
        RTFSetFont(kFontArial);
        newMult = 1.0f;
        break;
    }

    if (newMult != fGSFontSizeMult) {
        fGSFontSizeMult = newMult;
        RTFSetGSFontSize(fPreMultPointSize);
    }
}

/*
 * Set the font size of a IIgs font.  We factor the size multiplier in.
 *
 * BUG: we should track the state of the "underline" mode, and turn it
 * on and off based on the font size (8-point fonts aren't underlined).
 */
void ReformatText::RTFSetGSFontSize(int points)
{
    RTFSetFontSize((int) roundf(points * fGSFontSizeMult));

    fPreMultPointSize = points;
}

/*
 * Set bold/italic/underline etc.
 *
 * Note that "Teach" does not show underlining on text that is 8 points
 * or smaller.  We have to emulate this behavior or some documents, such
 * as ModZap's "MZ.Manual", look terrible.
 *
 * Set the font size before calling here.
 *
 * Some characters, such as '=' in Shaston 8, look the same in
 * bold as they do in plain.  This doesn't hold true for Windows
 * fonts, so we're going to look different in some circumstances.
 */
void ReformatText::RTFSetGSFontStyle(uint8_t qdStyle)
{
    if (!fUseRTF)
        return;

    if ((qdStyle & kQDStyleBold) != 0) {
        RTFBoldOn();
    } else {
        RTFBoldOff();
    }
    if ((qdStyle & kQDStyleItalic) != 0) {
        RTFItalicOn();
    } else {
        RTFItalicOff();
    }
    if ((qdStyle & kQDStyleUnderline) != 0 && fPreMultPointSize > 8) {
        RTFUnderlineOn();
    } else {
        RTFUnderlineOff();
    }
    if ((qdStyle & kQDStyleOutline) != 0) {
        RTFOutlineOn();
    } else {
        RTFOutlineOff();
    }
    if ((qdStyle & kQDStyleShadow) != 0) {
        RTFShadowOn();
    } else {
        RTFShadowOff();
    }
    if ((qdStyle & kQDStyleSuperscript) != 0) {
        RTFSuperscriptOn();
    } else {
        RTFSuperscriptOff();
    }
    if ((qdStyle & kQDStyleSubscript) != 0) {
        RTFSubscriptOn();
    } else {
        RTFSubscriptOff();
    }
}



#if 0
void
ReformatText::RTFProportionalOn(void) {
    if (fUseRTF)
        BufPrintf("\\f%d ", kFontTimesRoman);
}
void
ReformatText::RTFProportionalOff(void) {
    if (fUseRTF)
        BufPrintf("\\f%d ", kFontCourierNew);
}
#endif


/*
 * Convert the EOL markers in a buffer.  The output is written to the work
 * buffer.  The input buffer may be CR, LF, or CRLF.
 *
 * If "stripHiBits" is set, the high bit of each character is cleared before
 * the value is considered.
 */
void ReformatText::ConvertEOL(const uint8_t* srcBuf, long srcLen,
    bool stripHiBits)
{
    /* Compatibility - assume we're not stripping nulls */
    ConvertEOL(srcBuf, srcLen, stripHiBits, false);
}

/*
 * Convert the EOL markers in a buffer.  The output is written to the work
 * buffer.  The input buffer may be CR, LF, or CRLF.
 *
 * If "stripHiBits" is set, the high bit of each character is cleared before
 * the value is considered.
 *
 * If "stripNulls" is true, no null values will make it through.
 */
void ReformatText::ConvertEOL(const uint8_t* srcBuf, long srcLen,
    bool stripHiBits, bool stripNulls)
{
    uint8_t ch;
    int mask;

    assert(!fUseRTF);   // else we have to use RTFPrintChar

    if (stripHiBits)
        mask = 0x7f;
    else
        mask = 0xff;

    /*
     * Could probably speed this up by taking things a line at a time,
     * but this is fast enough and much more straightforward.
     */
    while (srcLen) {
        ch = (*srcBuf++) & mask;
        srcLen--;

        if (ch == '\r') {
            /* got CR, check for CRLF */
            if (srcLen != 0 && ((*srcBuf) & mask) == '\n') {
                srcBuf++;
                srcLen--;
            }
            BufPrintf("\r\n");
        } else if (ch == '\n') {
            BufPrintf("\r\n");
        } else {
            /* Strip out null bytes if requested */
            if ((stripNulls && ch != 0x00) || !stripNulls)
                BufPrintf("%c", ch);
        }
    }
}

/*
 * Write a hex dump into the buffer.
 */
void ReformatText::BufHexDump(const uint8_t* srcBuf, long srcLen)
{
    const uint8_t* origSrcBuf = srcBuf;
    char chBuf[17];
    int i, remLen;

    ASSERT(srcBuf != NULL);
    ASSERT(srcLen >= 0);

    chBuf[16] = '\0';

    while (srcLen > 0) {
        BufPrintf("%08lx: ", srcBuf - origSrcBuf);

        if (srcLen >= 16) {
            if (!fUseRTF) {
                /* the really easy (and relatively fast) way */
                BufPrintf("%02x %02x %02x %02x %02x %02x %02x %02x "
                          "%02x %02x %02x %02x %02x %02x %02x %02x ",
                    srcBuf[0], srcBuf[1], srcBuf[2], srcBuf[3],
                    srcBuf[4], srcBuf[5], srcBuf[6], srcBuf[7],
                    srcBuf[8], srcBuf[9], srcBuf[10], srcBuf[11],
                    srcBuf[12], srcBuf[13], srcBuf[14], srcBuf[15]);
            } else {
                /* the fairly easy (and fairly fast) way */
                RTFBoldOn();
                BufPrintf("%02x %02x %02x %02x ",
                    srcBuf[0], srcBuf[1], srcBuf[2], srcBuf[3]);
                RTFBoldOff();
                BufPrintf("%02x %02x %02x %02x ",
                    srcBuf[4], srcBuf[5], srcBuf[6], srcBuf[7]);
                RTFBoldOn();
                BufPrintf("%02x %02x %02x %02x ",
                    srcBuf[8], srcBuf[9], srcBuf[10], srcBuf[11]);
                RTFBoldOff();
                BufPrintf("%02x %02x %02x %02x ",
                    srcBuf[12], srcBuf[13], srcBuf[14], srcBuf[15]);
            }
        } else {
            /* the not-so-easy (and not-so-fast) way */
            remLen = srcLen;

            for (i = 0; i < remLen; i++) {
                if (i == 0 || i == 8)
                    RTFBoldOn();
                else if (i == 4 || i == 12)
                    RTFBoldOff();
                BufPrintf("%02x ", srcBuf[i]);
            }
            RTFBoldOff();
            for ( ; i < 16; i++)
                BufPrintf("   ");

            /* blank out the char buf, since we're only filling part in */
            for (i = 0; i < 16; i++)
                chBuf[i] = ' ';
        }

        bool hosed = false;
        remLen = srcLen;
        if (remLen > 16)
            remLen = 16;
        int i;
        for (i = 0; i < remLen; i++) {
            chBuf[i] = PrintableChar(srcBuf[i]);
            if (fUseRTF &&
                (chBuf[i] == '\\' || chBuf[i] == '{' || chBuf[i] == '}'))
            {
                hosed = true;
                break;
            }
        }

        if (!hosed) {
            BufPrintf(" %s", chBuf);
        } else {
            /* escaped chars in RTF mode; have to do this one the hard way */
            ASSERT(fUseRTF);
            BufPrintf(" ");
            for (i = 0; i < remLen; i++) {
                RTFPrintChar(srcBuf[i]);
            }
        }

        RTFNewPara();

        srcBuf += 16;
        srcLen -= 16;
    }
}

// Thanks to http://hoop-la.ca/apple2/docs/mousetext/unicode.html for Unicode.
// Thanks to Hugh Hood for extracting the ASCII conversions from SEG.WP.
static const struct {
    uint32_t unicode;       // UTF-16 single code point or surrogate pair
    int8_t ascii;           // 7-bit ASCII
} gMouseTextConv[32] = {
    { 0xd83cdf4e, '@' },    // 00 U+1f34e RED APPLE
    { 0xd83cdf4f, '@' },    // 01 U+1f34f GREEN APPLE
    //{ 0x00002316, '^' },    // 02 U+2316 POSITION INDICATOR
    { 0x000025c4, '^' },    // 02 U+25c4 BLACK LEFT-POINTING POINTER
    { 0x000023f3, '&' },    // 03 U+23f3 HOURGLASS WITH FLOWING SAND
    { 0x00002713, '\'' },   // 04 U+2713 CHECK MARK
    { 0x00002705, '\'' },   // 05 U+2705 WHITE HEAVY CHECK MARK
    { 0x000023ce, '/' },    // 06 U+23ce RETURN SYMBOL
    //{ 0xd83cdf54, ':' },    // 07 U+1f354 HAMBURGER
    { 0x00002630, ':' },    // 07 U+2630 TRIGRAM FOR HEAVEN (actually want 4 lines, not 3)
    { 0x00002190, '<' },    // 08 U+2190 LEFTWARDS ARROW
    { 0x00002026, '_' },    // 09 U+2026 HORIZONTAL ELLIPSIS
    { 0x00002193, 'v' },    // 0a U+2193 DOWNWARDS ARROW
    { 0x00002191, '^' },    // 0b U+2191 UPWARDS ARROW
    { 0x00002594, '-' },    // 0c U+2594 UPPER ONE EIGHTH BLOCK
    { 0x000021b5, '/' },    // 0d U+21b5 DOWNWARDS ARROW WITH CORNER LEFTWARDS
    { 0x00002589, '$' },    // 0e U+2589 LEFT SEVEN EIGHTHS BLOCK
    { 0x000021e4, '{' },    // 0f U+21e4 LEFTWARDS ARROW TO BAR
    { 0x000021e5, '}' },    // 10 U+21e5 RIGHTWARDS ARROW TO BAR
    { 0x00002913, 'v' },    // 11 U+2913 DOWNWARDS ARROW TO BAR
    { 0x00002912, '^' },    // 12 U+2912 UPWARDS ARROW TO BAR
    { 0x00002500, '-' },    // 13 U+2500 BOX DRAWINGS LIGHT HORIZONTAL
    { 0x0000231e, 'L' },    // 14 U+231e BOTTOM LEFT CORNER
    { 0x00002192, '>' },    // 15 U+2192 UPWARDS ARROW TO BAR
    //{ 0xd83dde7e, '*' },    // 16 U+1f67e CHECKER BOARD
    { 0x00002591, '*' },    // 16 U+2591 LIGHT SHADE
    //{ 0xd83dde7f, '*' },    // 17 U+1f67f REVERSE CHECKER BOARD
    { 0x00002592, '*' },    // 17 U+2592 MEDIUM SHADE
    { 0xd83ddcc1, '[' },    // 18 U+1f4c1 FILE FOLDER
    { 0xd83ddcc2, ']' },    // 19 U+1f4c2 OPEN FILE FOLDER
    { 0x00002595, '|' },    // 1a U+2595 RIGHT ONE EIGHTH BLOCK
    { 0x00002666, '#' },    // 1b U+2666 BLACK DIAMOND SUIT
    { 0x0000203e, '=' },    // 1c U+203e OVERLINE -- wrong, want top/bottom
    { 0x0000256c, '#' },    // 1d U+256c BOX DRAWINGS DOUBLE VERTICAL AND HORIZONTAL
    //{ 0xd83ddcbe, 'O' },    // 1e U+1f4be FLOPPY DISK
    { 0x000022a1, 'O' },    // 1e U+22a1 SQUARED DOT OPERATOR (seems better than 25a3)
    { 0x0000258f, '|' },    // 1f U+258f LEFT ONE EIGHTH BLOCK
};
void ReformatText::MouseTextToUTF16(uint8_t mtVal, uint16_t* pLow, uint16_t* pHigh) {
    ASSERT(mtVal < 32);

    *pLow = gMouseTextConv[mtVal].unicode & 0xffff;
    *pHigh = gMouseTextConv[mtVal].unicode >> 16;
}
int8_t ReformatText::MouseTextToASCII(uint8_t mtVal) {
    ASSERT(mtVal < 32);
    return gMouseTextConv[mtVal].ascii;
}


/*
 * ==========================================================================
 *      ReformatGraphics
 * ==========================================================================
 */

/*
 * Initialize the Apple II color palette, used for Hi-Res and DHR
 * conversions.  Could also be used for lo-res mode.
 */
void ReformatGraphics::InitPalette(void)
{
    ASSERT(kPaletteSize == 16);

    static const RGBQUAD stdPalette[kPaletteSize] = {
        /* blue, green, red, reserved */
        { 0x00, 0x00, 0x00 },   // $0 black
        { 0x33, 0x00, 0xdd },   // $1 red (magenta)
        { 0x99, 0x00, 0x00 },   // $2 dark blue
        { 0xdd, 0x22, 0xdd },   // $3 purple (violet)
        { 0x22, 0x77, 0x00 },   // $4 dark green
        { 0x55, 0x55, 0x55 },   // $5 grey1 (dark)
        { 0xff, 0x22, 0x22 },   // $6 medium blue
        { 0xff, 0xaa, 0x66 },   // $7 light blue
        { 0x00, 0x55, 0x88 },   // $8 brown
        { 0x00, 0x66, 0xff },   // $9 orange
        { 0xaa, 0xaa, 0xaa },   // $A grey2 (light)
        { 0x88, 0x99, 0xff },   // $B pink
        { 0x00, 0xdd, 0x11 },   // $C green (a/k/a light green)
        { 0x00, 0xff, 0xff },   // $D yellow
        { 0x99, 0xff, 0x44 },   // $E aqua
        { 0xff, 0xff, 0xff },   // $F white
    };

    memcpy(fPalette, stdPalette, sizeof(fPalette));
}

/*
 * Stuff out DIB into the output fields, and set the appropriate flags.
 */
void ReformatGraphics::SetResultBuffer(ReformatOutput* pOutput, MyDIBitmap* pDib)
{
    ASSERT(pOutput != NULL);
    ASSERT(pDib != NULL);
    pOutput->SetOutputKind(ReformatOutput::kOutputBitmap);
    pOutput->SetDIB(pDib);
}

/*
 * Unpack the Apple PackBytes format.
 *
 * Format is:
 *  <flag><data> ...
 *
 * Flag values (first 6 bits of flag byte):
 *  00xxxxxx: (0-63) 1 to 64 bytes follow, all different
 *  01xxxxxx: (0-63) 1 to 64 repeats of next byte
 *  10xxxxxx: (0-63) 1 to 64 repeats of next 4 bytes
 *  11xxxxxx: (0-63) 1 to 64 repeats of next byte taken as 4 bytes
 *              (as in 10xxxxxx case)
 *
 * Pass the destination buffer in "dst", source buffer in "src", source
 * length in "srcLen", and expected sizes of output in "dstRem".
 *
 * Returns the number of bytes unpacked on success, negative if the buffer is
 * overfilled.
 */
int ReformatGraphics::UnpackBytes(uint8_t* dst, const uint8_t* src,
    long dstRem, long srcLen)
{
    const uint8_t* origDst = dst;

    while (srcLen > 0) {
        uint8_t flag = *src++;
        int count = (flag & 0x3f) +1;
        uint8_t val;
        uint8_t valSet[4];
        int i;

        srcLen--;
    
        switch (flag & 0xc0) {
        case 0x00:
            for (i = 0; i < count; i++) {
                if (srcLen == 0 || dstRem == 0) {
                    LOGI(" SHR unpack overrun1 (srcLen=%ld dstRem=%ld)",
                        srcLen, dstRem);
                    return -1;
                }
                *dst++ = *src++;
                srcLen--;
                dstRem--;
            }
            break;
        case 0x40:
            //if (count != 3 || count != 5 || count != 6 || count != 7) {
            //  LOGI(" SHR unpack funky len %d?", count);
            //}
            if (srcLen == 0) {
                LOGI(" SHR unpack underrun2");
                return -1;
            }
            val = *src++;
            srcLen--;
            for (i = 0; i < count; i++) {
                if (dstRem == 0) {
                    LOGI(" SHR unpack overrun2 (srcLen=%d, i=%d of %d)",
                        srcLen, i, count);
                    return -1;
                }
                *dst++ = val;
                dstRem--;
            }
            break;
        case 0x80:
            if (srcLen < 4) {
                LOGI(" SHR unpack underrun3");
                return -1;
            }
            valSet[0] = *src++;
            valSet[1] = *src++;
            valSet[2] = *src++;
            valSet[3] = *src++;
            srcLen -= 4;
            for (i = 0; i < count; i++) {
                if (dstRem < 4) {
                    LOGI(" SHR unpack overrun3 (srcLen=%ld dstRem=%ld)",
                        srcLen, dstRem);
                    return -1;
                }
                *dst++ = valSet[0];
                *dst++ = valSet[1];
                *dst++ = valSet[2];
                *dst++ = valSet[3];
                dstRem -= 4;
            }
            break;
        case 0xc0:
            if (srcLen == 0) {
                LOGI(" SHR unpack underrun4");
                return -1;
            }
            val = *src++;
            srcLen--;
            for (i = 0; i < count; i++) {
                if (dstRem < 4) {
                    LOGI(" SHR unpack overrun4 (srcLen=%ld dstRem=%ld count=%d)",
                        srcLen, dstRem, count);
                    return -1;
                }
                *dst++ = val;
                *dst++ = val;
                *dst++ = val;
                *dst++ = val;
                dstRem -= 4;
            }
            break;
        default:
            ASSERT(false);
            break;
        }
    }

    ASSERT(srcLen == 0);

    if (false) {
        /* require that we completely fill the buffer */
        if (dstRem != 0) {
            LOGI(" SHR unpack dstRem at %d", dstRem);
            return -1;
        }
    }

    return dst - origDst;
}

/*
 * Unpack Macintosh PackBits format.  See Technical Note TN1023.
 *
 * Read a byte.
 * If the high bit is set, count is 2s complement +1 (i.e. count = (-byte)+1).
 *   Read the next byte, then write that byte 'count' times.
 * If the high bit is clear, count is 1+value (i.e. count = byte+1).  Read and
 *   copy that many bytes.
 * After "destLen" bytes have been written, return (even if in the middle of
 * a run).
 *
 * NOTE: if the count byte is 0x80, Apple says it's an invalid value and
 * should be skipped over.  Use the following byte as the count byte.  This
 * is probably because PackBits is only supposed to crunch 127 bytes, though
 * that suggests 0x81 and 0x7f are also impossible.
 *
 * We have to watch for underruns on the input and overruns on the output.
 */
void ReformatGraphics::UnPackBits(const uint8_t** pSrcBuf, long* pSrcLen,
    uint8_t** pOutPtr, long dstLen, uint8_t xorVal)
{
    const uint8_t* srcBuf = *pSrcBuf;
    long length = *pSrcLen;
    uint8_t* outPtr = *pOutPtr;
    int pixByte = 0;

    while (pixByte < dstLen && length > 0) {
        uint8_t countByte;
        int count;
        
        countByte = *srcBuf++;
        length--;
        if (countByte & 0x80) {
            /* RLE string */
            uint8_t ch;
            count = (countByte ^ 0xff)+1 +1;
            ch = *srcBuf++;
            length--;
            while (count-- && pixByte < dstLen) {
                *outPtr++ = ch ^ xorVal;
                pixByte++;
            }
        } else {
            /* series of bytes */
            count = countByte +1;
            while (count && pixByte < dstLen && length > 0) {
                *outPtr++ = *srcBuf++ ^ xorVal;
                count--;
                length--;
                pixByte++;
            }
        }
    }
    if (pixByte != 72) {
        /* can happen if we run out of input early */
        LOGI("  MP unexpected pixByte=%d", pixByte);
        /* keep going */
    }

    *pSrcBuf = srcBuf;
    *pSrcLen = length;
    *pOutPtr = outPtr;
}
