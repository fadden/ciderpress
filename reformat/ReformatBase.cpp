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


static float roundf(float val)
{
	if (val < 0.5f)
		return ::floorf(val);
	else
		return ::ceilf(val);
}

/*
 * ==========================================================================
 *		ReformatText
 * ==========================================================================
 */

/*
 * Convert IIgs high-ASCII characters to Windows equivalents (when
 * available).
 *
 * Also found this:
 * http://www.unicode.org/Public/MAPPINGS/VENDORS/APPLE/ROMAN.TXT
 */
const int kUnk = 0x3f;		// for unmappable chars, use '?'

/*static*/ const unsigned char ReformatText::kGSCharConv[128] = {
	0xc4,	// 0x80 A + umlaut (diaeresis?)
	0xc5,	// 0x81 A + overcircle
	0xc7,	// 0x82 C + cedilla
	0xc9,	// 0x83 E + acute
	0xd1,	// 0x84 N + tilde
	0xd6,	// 0x85 O + umlaut
	0xdc,	// 0x86 U + umlaut
	0xe1,	// 0x87 a + acute
	0xe0,	// 0x88 a + grave
	0xe2,	// 0x89 a + circumflex
	0xe4,	// 0x8a a + umlaut
	0xe3,	// 0x8b a + tilde
	0xe5,	// 0x8c a + overcircle
	0xe7,	// 0x8d c + cedilla
	0xe9,	// 0x8e e + acute
	0xe8,	// 0x8f e + grave
	0xea,	// 0x90 e + circumflex
	0xeb,	// 0x91 e + umlaut
	0xed,	// 0x92 i + acute
	0xec,	// 0x93 i + grave
	0xee,	// 0x94 i + circumflex
	0xef,	// 0x95 i + umlaut
	0xf1,	// 0x96 n + tilde
	0xf3,	// 0x97 o + acute
	0xf2,	// 0x98 o + grave
	0xf4,	// 0x99 o + circumflex
	0xf6,	// 0x9a o + umlaut
	0xf5,	// 0x9b o + tilde
	0xfa,	// 0x9c u + acute
	0xf9,	// 0x9d u + grave
	0xfb,	// 0x9e u + circumflex
	0xfc,	// 0x9f u + umlaut
	0x87,	// 0xa0 double cross (dagger)
	0xb0,	// 0xa1 degrees
	0xa2,	// 0xa2 cents
	0xa3,	// 0xa3 pounds (UK$)
	0xa7,	// 0xa4 section start
	0x95,	// 0xa5 small square (bullet)  [using fat bullet]
	0xb6,	// 0xa6 paragraph (pilcrow)
	0xdf,	// 0xa7 curly B (latin small letter sharp S)
	0xae,	// 0xa8 raised 'R' (registered)
	0xa9,	// 0xa9 raised 'C' (copyright)
	0x99,	// 0xaa raised 'TM' (trademark)
	0xb4,	// 0xab acute accent
	0xa8,	// 0xac umlaut (diaeresis)
	kUnk,	// 0xad not-equal
	0xc6,	// 0xae merged AE
	0xd8,	// 0xaf O + slash (upper-case nil?)
	kUnk,	// 0xb0 infinity
	0xb1,	// 0xb1 +/-
	kUnk,	// 0xb2 <=
	kUnk,	// 0xb3 >=
	0xa5,	// 0xb4 Yen (Japan$)
	0xb5,	// 0xb5 mu (micro)
	kUnk,	// 0xb6 delta (partial differentiation) [could use D-bar 0xd0]
	kUnk,	// 0xb7 epsilon (N-ary summation) [could use C-double-bar 0x80]
	kUnk,	// 0xb8 PI (N-ary product)
	kUnk,	// 0xb9 pi
	kUnk,	// 0xba integral
	0xaa,	// 0xbb a underbar (feminine ordinal)  [using raised a]
	0xba,	// 0xbc o underbar (masculine ordinal)  [using raised o]
	kUnk,	// 0xbd omega (Ohm)
	0xe6,	// 0xbe merged ae
	0xf8,	// 0xbf o + slash (lower-case nil?)
	0xbf,	// 0xc0 upside-down question mark
	0xa1,	// 0xc1 upside-down exclamation point
	0xac,	// 0xc2 rotated L ("not" sign)
	0xb7,	// 0xc3 checkmark (square root) [using small bullet]
	0x83,	// 0xc4 script f
	kUnk,	// 0xc5 approximately equal
	kUnk,	// 0xc6 delta (triangle / increment)
	0xab,	// 0xc7 much less than
	0xbb,	// 0xc8 much greater than
	0x85,	// 0xc9 ellipsis
	0xa0,	// 0xca blank (sticky space)
	0xc0,	// 0xcb A + grave
	0xc3,	// 0xcc A + tilde
	0xd5,	// 0xcd O + tilde
	0x8c,	// 0xce merged OE
	0x9c,	// 0xcf merged oe
	0x96,	// 0xd0 short hyphen (en dash)
	0x97,	// 0xd1 long hyphen (em dash)
	0x93,	// 0xd2 smart double-quote start
	0x94,	// 0xd3 smart double-quote end
	0x91,	// 0xd4 smart single-quote start
	0x92,	// 0xd5 smart single-quote end
	0xf7,	// 0xd6 divide
	0xa4,	// 0xd7 diamond (lozenge)  [using spiky circle]
	0xff,	// 0xd8 y + umlaut
	// [nothing below here is part of standard Windows-ASCII?]
	// remaining descriptions based on hfsutils' "charset.txt"
	kUnk,	// 0xd9 Y + umlaut
	kUnk,	// 0xda fraction slash
	kUnk,	// 0xdb currency sign
	kUnk,	// 0xdc single left-pointing angle quotation mark
	kUnk,	// 0xdd single right-pointing angle quotation mark
	kUnk,	// 0xde merged fi
	kUnk,	// 0xdf merged FL
	kUnk,	// 0xe0 double dagger
	kUnk,	// 0xe1 middle dot
	kUnk,	// 0xe2 single low-9 quotation mark
	kUnk,	// 0xe3 double low-9 quotation mark
	kUnk,	// 0xe4 per mille sign
	kUnk,	// 0xe5 A + circumflex
	kUnk,	// 0xe6 E + circumflex
	kUnk,	// 0xe7 A + acute accent
	kUnk,	// 0xe8 E + diaeresis
	kUnk,	// 0xe9 E + grave accent
	kUnk,	// 0xea I + acute accent
	kUnk,	// 0xeb I + circumflex
	kUnk,	// 0xec I + diaeresis
	kUnk,	// 0xed I + grave accent
	kUnk,	// 0xee O + acute accent
	kUnk,	// 0xef O + circumflex
	kUnk,	// 0xf0 apple logo
	kUnk,	// 0xf1 O + grave accent
	kUnk,	// 0xf2 U + acute accent
	kUnk,	// 0xf3 U + circumflex
	kUnk,	// 0xf4 U + grave accent
	kUnk,	// 0xf5 i without dot
	kUnk,	// 0xf6 modifier letter circumflex accent
	kUnk,	// 0xf7 small tilde
	kUnk,	// 0xf8 macron
	kUnk,	// 0xf9 breve
	kUnk,	// 0xfa dot above
	kUnk,	// 0xfb ring above
	kUnk,	// 0xfc cedilla
	kUnk,	// 0xfd double acute accent
	kUnk,	// 0xfe ogonek
	kUnk,	// 0xff caron
};

/*
 * Quick sanity check on contents of array.
 *
 * No two characters should map to the same thing.  This isn't vital, but
 * if we want to have a reversible transformation someday, it'll make our
 * lives easier then.
 */
void
ReformatText::CheckGSCharConv(void)
{
#ifdef _DEBUG
	bool test[256];
	int i;

	memset(test, 0, sizeof(test));
	for (i = 0; i < sizeof(kGSCharConv); i++) {
		if (test[kGSCharConv[i]] && kGSCharConv[i] != kUnk) {
			WMSG3("Character used twice: 0x%02x at %d (0x%02x)\n",
				kGSCharConv[i], i, i+128);
			assert(false);
		}
		test[kGSCharConv[i]] = true;
	}
#endif
}


/*
 * Set the output format and buffer.
 *
 * Clears our work buffer pointer so we don't free it.
 */
void
ReformatText::SetResultBuffer(ReformatOutput* pOutput, bool multiFont)
{
	char* buf;
	long len;
	fExpBuf.SeizeBuffer(&buf, &len);
	pOutput->SetTextBuf(buf, len, true);

	if (pOutput->GetTextBuf() == nil) {
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
		 * worry about nil pointers.
		 */
		pOutput->SetOutputKind(ReformatOutput::kOutputRaw);
		WMSG0("ReformatText returning a nil pointer\n");
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
void
ReformatText::RTFBegin(int flags)
{
//	static const char* rtfHdr =
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
void
ReformatText::RTFEnd(void) { if (fUseRTF) BufPrintf("}\r\n%c", '\0'); }

/*
 * Output RTF paragraph definition marker.  Do this every time we change some
 * aspect of paragraph formatting, such as margins or justification.
 */
void
ReformatText::RTFSetPara(void)
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
	case kJustifyLeft:							break;
	case kJustifyRight:		BufPrintf("\\qr");	break;
	case kJustifyCenter:	BufPrintf("\\qc");	break;
	case kJustifyFull:		BufPrintf("\\qj");	break;
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
void
ReformatText::RTFNewPara(void)
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
void
ReformatText::RTFPageBreak(void)
{
	if (fUseRTF)
		BufPrintf("\\page ");
}

/*
 * RTF tab character.
 */
void
ReformatText::RTFTab(void)
{
	if (fUseRTF)
		BufPrintf("\\tab ");
}

/*
 * Minor formatting.
 */
void
ReformatText::RTFBoldOn(void)
{
	if (fBoldEnabled)
		return;
	if (fUseRTF) {
		BufPrintf("\\b ");
		fBoldEnabled = true;
	}
}
void
ReformatText::RTFBoldOff(void)
{
	if (!fBoldEnabled)
		return;
	if (fUseRTF) {
		BufPrintf("\\b0 ");
		fBoldEnabled = false;
	}
}
void
ReformatText::RTFItalicOn(void)
{
	if (fItalicEnabled)
		return;
	if (fUseRTF) {
		BufPrintf("\\i ");
		fItalicEnabled = true;
	}
}
void
ReformatText::RTFItalicOff(void)
{
	if (!fItalicEnabled)
		return;
	if (fUseRTF) {
		BufPrintf("\\i0 ");
		fItalicEnabled = false;
	}
}
void
ReformatText::RTFUnderlineOn(void)
{
	if (fUnderlineEnabled)
		return;
	if (fUseRTF) {
		BufPrintf("\\ul ");
		fUnderlineEnabled = true;
	}
}
void
ReformatText::RTFUnderlineOff(void)
{
	if (!fUnderlineEnabled)
		return;
	if (fUseRTF) {
		BufPrintf("\\ulnone ");
		fUnderlineEnabled = false;
	}
}
void
ReformatText::RTFSubscriptOn(void)
{
	if (fSubscriptEnabled)
		return;
	if (fUseRTF) {
		BufPrintf("\\sub ");
		fSubscriptEnabled = true;
	}
}
void
ReformatText::RTFSubscriptOff(void)
{
	if (!fSubscriptEnabled)
		return;
	if (fUseRTF) {
		BufPrintf("\\nosupersub ");
		fSubscriptEnabled = false;
	}
}
void
ReformatText::RTFSuperscriptOn(void)
{
	if (fSuperscriptEnabled)
		return;
	if (fUseRTF) {
		BufPrintf("\\super ");
		fSuperscriptEnabled = true;
	}
}
void
ReformatText::RTFSuperscriptOff(void)
{
	if (!fSuperscriptEnabled)
		return;
	if (fUseRTF) {
		BufPrintf("\\nosupersub ");
		fSuperscriptEnabled = false;
	}
}
void
ReformatText::RTFSetColor(TextColor color)
{
	if (color == fTextColor)
		return;
	if (fUseRTF) {
		BufPrintf("\\cf%d ", color);
		fTextColor = color;
	}
}

/*
 * Change paragraph formatting.
 */
void
ReformatText::RTFParaLeft(void)
{
	if (fJustified != kJustifyLeft) {
		fJustified = kJustifyLeft;
		RTFSetPara();
	}
}
void
ReformatText::RTFParaRight(void)
{
	if (fJustified != kJustifyRight) {
		fJustified = kJustifyRight;
		RTFSetPara();
	}
}
void
ReformatText::RTFParaCenter(void)
{
	if (fJustified != kJustifyCenter) {
		fJustified = kJustifyCenter;
		RTFSetPara();
	}
}
void
ReformatText::RTFParaJustify(void)
{
	if (fJustified != kJustifyFull) {
		fJustified = kJustifyFull;
		RTFSetPara();
	}
}

/*
 * Page margins, in 1/10th inches.
 */
void
ReformatText::RTFLeftMargin(int margin)
{
	//WMSG1("+++ Left margin now %d\n", margin);
	fLeftMargin = margin;
	RTFSetPara();
}
void
ReformatText::RTFRightMargin(int margin)
{
	//WMSG1("+++ Right margin now %d\n", margin);
	fRightMargin = margin;
	RTFSetPara();
}


/*
 * Switch to a different font size.
 */
void
ReformatText::RTFSetFontSize(int points)
{
	if (fUseRTF && fPointSize != points)
		BufPrintf("\\fs%d ", points * 2);
	fPointSize = points;
}
/*
 * Switch to a different font.
 */
void
ReformatText::RTFSetFont(RTFFont font)
{
	if (fUseRTF)
		BufPrintf("\\f%d ", font);
}

/*
 * Set the font by specifying a IIgs QuickDraw II font family number.
 */
void
ReformatText::RTFSetGSFont(unsigned short family)
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
		WMSG1("Unrecognized font family 0x%04x\n", family);
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
void
ReformatText::RTFSetGSFontSize(int points)
{
	RTFSetFontSize((int) roundf(points * fGSFontSizeMult));

	fPreMultPointSize = points;
}

/*
 * Set bold/italic/underline.  "Teach" ignores you if you try to
 * underline text smaller than 8 points, but if you leave the mode
 * on from a previous block it will act like it wants to underline
 * text but not actually do it.  We have to emulate this behavior,
 * or some documents (e.g. "MZ.MANUAL") look terrible.
 *
 * Set the font size before calling here.
 *
 * Some characters, such as '=' in Shaston 8, look the same in
 * bold as they do in plain.  This doesn't hold true for Windows
 * fonts, so we're going to look different in some circumstances.
 */
void
ReformatText::RTFSetGSFontStyle(unsigned char qdStyle)
{
	if (!fUseRTF)
		return;

	if ((qdStyle & kQDStyleBold) != 0)
		RTFBoldOn();
	else
		RTFBoldOff();
	if ((qdStyle & kQDStyleItalic) != 0)
		RTFItalicOn();
	else
		RTFItalicOff();
	if ((qdStyle & kQDStyleUnderline) != 0 && fPreMultPointSize > 8)
		RTFUnderlineOn();
	else
		RTFUnderlineOff();
	if ((qdStyle & kQDStyleSuperscript) != 0)
		RTFSuperscriptOn();
	else
		RTFSuperscriptOff();
	if ((qdStyle & kQDStyleSubscript) != 0)
		RTFSubscriptOn();
	else
		RTFSubscriptOff();
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
void
ReformatText::ConvertEOL(const unsigned char* srcBuf, long srcLen,
	bool stripHiBits)
{
	unsigned char ch;
	int mask;

	assert(!fUseRTF);	// else we have to use RTFPrintChar

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
			BufPrintf("%c", ch);
		}
	}
}

/*
 * Write a hex dump into the buffer.
 */
void
ReformatText::BufHexDump(const unsigned char* srcBuf, long srcLen)
{
	const unsigned char* origSrcBuf = srcBuf;
	char chBuf[17];
	int i, remLen;

	ASSERT(srcBuf != nil);
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


/*
 * ==========================================================================
 *		ReformatGraphics
 * ==========================================================================
 */

/*
 * Initialize the Apple II color palette, used for Hi-Res and DHR
 * conversions.  Could also be used for lo-res mode.
 */
void
ReformatGraphics::InitPalette(void)
{
	ASSERT(kPaletteSize == 16);

	static const RGBQUAD stdPalette[kPaletteSize] = {
		/* blue, green, red, reserved */
		{ 0x00, 0x00, 0x00 },	// $0 black
		{ 0x33, 0x00, 0xdd },	// $1 red (magenta)
		{ 0x99, 0x00, 0x00 },	// $2 dark blue
		{ 0xdd, 0x22, 0xdd },	// $3 purple (violet)
		{ 0x22, 0x77, 0x00 },	// $4 dark green
		{ 0x55, 0x55, 0x55 },	// $5 grey1 (dark)
		{ 0xff, 0x22, 0x22 },	// $6 medium blue
		{ 0xff, 0xaa, 0x66 },	// $7 light blue
		{ 0x00, 0x55, 0x88 },	// $8 brown
		{ 0x00, 0x66, 0xff },	// $9 orange
		{ 0xaa, 0xaa, 0xaa },	// $A grey2 (light)
		{ 0x88, 0x99, 0xff },	// $B pink
		{ 0x00, 0xdd, 0x11 },	// $C green (a/k/a light green)
		{ 0x00, 0xff, 0xff },	// $D yellow
		{ 0x99, 0xff, 0x44 },	// $E aqua
		{ 0xff, 0xff, 0xff },	// $F white
	};

	memcpy(fPalette, stdPalette, sizeof(fPalette));
}

/*
 * Stuff out DIB into the output fields, and set the appropriate flags.
 */
void
ReformatGraphics::SetResultBuffer(ReformatOutput* pOutput, MyDIBitmap* pDib)
{
	ASSERT(pOutput != nil);
	ASSERT(pDib != nil);
	pOutput->SetOutputKind(ReformatOutput::kOutputBitmap);
	pOutput->SetDIB(pDib);
}


/*
 * Unpack the Apple PackBytes format.
 *
 * Format is:
 *	<flag><data> ...
 *
 * Flag values (first 6 bits of flag byte):
 *	00xxxxxx: (0-63) 1 to 64 bytes follow, all different
 *	01xxxxxx: (0-63) 1 to 64 repeats of next byte
 *	10xxxxxx: (0-63) 1 to 64 repeats of next 4 bytes
 *	11xxxxxx: (0-63) 1 to 64 repeats of next byte taken as 4 bytes
 *				(as in 10xxxxxx case)
 *
 * Pass the destination buffer in "dst", source buffer in "src", source
 * length in "srcLen", and expected sizes of output in "dstRem".
 *
 * Returns 0 on success, nonzero if the buffer is overfilled or underfilled.
 */
int
ReformatGraphics::UnpackBytes(unsigned char* dst, const unsigned char* src,
	long dstRem, long srcLen)
{
	while (srcLen > 0) {
		unsigned char flag = *src++;
		int count = (flag & 0x3f) +1;
		unsigned char val;
		unsigned char valSet[4];
		int i;

		srcLen--;
	
		switch (flag & 0xc0) {
		case 0x00:
			for (i = 0; i < count; i++) {
				if (srcLen == 0 || dstRem == 0) {
					WMSG2(" SHR unpack overrun1 (srcLen=%ld dstRem=%ld)\n",
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
			//	WMSG1(" SHR unpack funky len %d?\n", count);
			//}
			if (srcLen == 0) {
				WMSG0(" SHR unpack underrun2\n");
				return -1;
			}
			val = *src++;
			srcLen--;
			for (i = 0; i < count; i++) {
				if (dstRem == 0) {
					WMSG3(" SHR unpack overrun2 (srcLen=%d, i=%d of %d)\n",
						srcLen, i, count);
					return -1;
				}
				*dst++ = val;
				dstRem--;
			}
			break;
		case 0x80:
			if (srcLen < 4) {
				WMSG0(" SHR unpack underrun3\n");
				return -1;
			}
			valSet[0] = *src++;
			valSet[1] = *src++;
			valSet[2] = *src++;
			valSet[3] = *src++;
			srcLen -= 4;
			for (i = 0; i < count; i++) {
				if (dstRem < 4) {
					WMSG2(" SHR unpack overrun3 (srcLen=%ld dstRem=%ld)\n",
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
				WMSG0(" SHR unpack underrun4\n");
				return -1;
			}
			val = *src++;
			srcLen--;
			for (i = 0; i < count; i++) {
				if (dstRem < 4) {
					WMSG3(" SHR unpack overrun4 (srcLen=%ld dstRem=%ld count=%d)\n",
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

	/* require that we completely fill the buffer */
	if (dstRem != 0) {
		WMSG1(" SHR unpack dstRem at %d\n", dstRem);
		return -1;
	}

	return 0;
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
void
ReformatGraphics::UnPackBits(const unsigned char** pSrcBuf, long* pSrcLen,
	unsigned char** pOutPtr, long dstLen, unsigned char xorVal)
{
	const unsigned char* srcBuf = *pSrcBuf;
	long length = *pSrcLen;
	unsigned char* outPtr = *pOutPtr;
	int pixByte = 0;

	while (pixByte < dstLen && length > 0) {
		unsigned char countByte;
		int count;
		
		countByte = *srcBuf++;
		length--;
		if (countByte & 0x80) {
			/* RLE string */
			unsigned char ch;
			count = (countByte ^ 0xff)+1 +1;
			ch = *srcBuf++;
			length--;
			while (count-- && pixByte < dstLen) {
				*outPtr++ = ch ^ xorVal;
				pixByte++;
			}
		} else {
			/* series of bytse */
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
		WMSG1("  MP unexpected pixByte=%d\n", pixByte);
		/* keep going */
	}

	*pSrcBuf = srcBuf;
	*pSrcLen = length;
	*pOutPtr = outPtr;
}
