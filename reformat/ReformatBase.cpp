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
 * Convert Mac OS Roman to Windows CP1252.
 */
const int kUnk = 0x3f;      // for unmappable chars, use '?'

/*static*/ const uint8_t ReformatText::kCP1252Conv[128] = {
    0xc4,   // 0x80 A + umlaut (diaeresis?)
    0xc5,   // 0x81 A + overcircle
    0xc7,   // 0x82 C + cedilla
    0xc9,   // 0x83 E + acute
    0xd1,   // 0x84 N + tilde
    0xd6,   // 0x85 O + umlaut
    0xdc,   // 0x86 U + umlaut
    0xe1,   // 0x87 a + acute
    0xe0,   // 0x88 a + grave
    0xe2,   // 0x89 a + circumflex
    0xe4,   // 0x8a a + umlaut
    0xe3,   // 0x8b a + tilde
    0xe5,   // 0x8c a + overcircle
    0xe7,   // 0x8d c + cedilla
    0xe9,   // 0x8e e + acute
    0xe8,   // 0x8f e + grave
    0xea,   // 0x90 e + circumflex
    0xeb,   // 0x91 e + umlaut
    0xed,   // 0x92 i + acute
    0xec,   // 0x93 i + grave
    0xee,   // 0x94 i + circumflex
    0xef,   // 0x95 i + umlaut
    0xf1,   // 0x96 n + tilde
    0xf3,   // 0x97 o + acute
    0xf2,   // 0x98 o + grave
    0xf4,   // 0x99 o + circumflex
    0xf6,   // 0x9a o + umlaut
    0xf5,   // 0x9b o + tilde
    0xfa,   // 0x9c u + acute
    0xf9,   // 0x9d u + grave
    0xfb,   // 0x9e u + circumflex
    0xfc,   // 0x9f u + umlaut
    0x87,   // 0xa0 double cross (dagger)
    0xb0,   // 0xa1 degrees
    0xa2,   // 0xa2 cents
    0xa3,   // 0xa3 pounds (UK$)
    0xa7,   // 0xa4 section start
    0x95,   // 0xa5 small square (bullet)  [using fat bullet]
    0xb6,   // 0xa6 paragraph (pilcrow)
    0xdf,   // 0xa7 curly B (latin small letter sharp S)
    0xae,   // 0xa8 raised 'R' (registered)
    0xa9,   // 0xa9 raised 'C' (copyright)
    0x99,   // 0xaa raised 'TM' (trademark)
    0xb4,   // 0xab acute accent
    0xa8,   // 0xac umlaut (diaeresis)
    kUnk,   // 0xad not-equal
    0xc6,   // 0xae merged AE
    0xd8,   // 0xaf O + slash (upper-case nil?)
    kUnk,   // 0xb0 infinity
    0xb1,   // 0xb1 +/-
    kUnk,   // 0xb2 <=
    kUnk,   // 0xb3 >=
    0xa5,   // 0xb4 Yen (Japan$)
    0xb5,   // 0xb5 mu (micro)
    kUnk,   // 0xb6 delta (partial differentiation) [could use D-bar 0xd0]
    kUnk,   // 0xb7 epsilon (N-ary summation) [could use C-double-bar 0x80]
    kUnk,   // 0xb8 PI (N-ary product)
    kUnk,   // 0xb9 pi
    kUnk,   // 0xba integral
    0xaa,   // 0xbb a underbar (feminine ordinal)  [using raised a]
    0xba,   // 0xbc o underbar (masculine ordinal)  [using raised o]
    kUnk,   // 0xbd omega (Ohm)
    0xe6,   // 0xbe merged ae
    0xf8,   // 0xbf o + slash (lower-case NULL?)
    0xbf,   // 0xc0 upside-down question mark
    0xa1,   // 0xc1 upside-down exclamation point
    0xac,   // 0xc2 rotated L ("not" sign)
    0xb7,   // 0xc3 checkmark (square root) [using small bullet]
    0x83,   // 0xc4 script f
    kUnk,   // 0xc5 approximately equal
    kUnk,   // 0xc6 delta (triangle / increment)
    0xab,   // 0xc7 much less than
    0xbb,   // 0xc8 much greater than
    0x85,   // 0xc9 ellipsis
    0xa0,   // 0xca blank (sticky space)
    0xc0,   // 0xcb A + grave
    0xc3,   // 0xcc A + tilde
    0xd5,   // 0xcd O + tilde
    0x8c,   // 0xce merged OE
    0x9c,   // 0xcf merged oe
    0x96,   // 0xd0 short hyphen (en dash)
    0x97,   // 0xd1 long hyphen (em dash)
    0x93,   // 0xd2 smart double-quote start
    0x94,   // 0xd3 smart double-quote end
    0x91,   // 0xd4 smart single-quote start
    0x92,   // 0xd5 smart single-quote end
    0xf7,   // 0xd6 divide
    0xa4,   // 0xd7 diamond (lozenge)  [using spiky circle]
    0xff,   // 0xd8 y + umlaut
    // [nothing below here is part of standard Windows-ASCII?]
    // remaining descriptions based on hfsutils' "charset.txt"
    kUnk,   // 0xd9 Y + umlaut
    kUnk,   // 0xda fraction slash
    kUnk,   // 0xdb currency sign
    kUnk,   // 0xdc single left-pointing angle quotation mark
    kUnk,   // 0xdd single right-pointing angle quotation mark
    kUnk,   // 0xde merged fi
    kUnk,   // 0xdf merged FL
    kUnk,   // 0xe0 double dagger
    kUnk,   // 0xe1 middle dot
    kUnk,   // 0xe2 single low-9 quotation mark
    kUnk,   // 0xe3 double low-9 quotation mark
    kUnk,   // 0xe4 per mille sign
    kUnk,   // 0xe5 A + circumflex
    kUnk,   // 0xe6 E + circumflex
    kUnk,   // 0xe7 A + acute accent
    kUnk,   // 0xe8 E + diaeresis
    kUnk,   // 0xe9 E + grave accent
    kUnk,   // 0xea I + acute accent
    kUnk,   // 0xeb I + circumflex
    kUnk,   // 0xec I + diaeresis
    kUnk,   // 0xed I + grave accent
    kUnk,   // 0xee O + acute accent
    kUnk,   // 0xef O + circumflex
    kUnk,   // 0xf0 apple logo
    kUnk,   // 0xf1 O + grave accent
    kUnk,   // 0xf2 U + acute accent
    kUnk,   // 0xf3 U + circumflex
    kUnk,   // 0xf4 U + grave accent
    kUnk,   // 0xf5 i without dot
    kUnk,   // 0xf6 modifier letter circumflex accent
    kUnk,   // 0xf7 small tilde
    kUnk,   // 0xf8 macron
    kUnk,   // 0xf9 breve
    kUnk,   // 0xfa dot above
    kUnk,   // 0xfb ring above
    kUnk,   // 0xfc cedilla
    kUnk,   // 0xfd double acute accent
    kUnk,   // 0xfe ogonek
    kUnk,   // 0xff caron
};

/*
 * Convert Mac OS Roman to Unicode.  Mapping comes from:
 *
 * http://www.unicode.org/Public/MAPPINGS/VENDORS/APPLE/ROMAN.TXT
 *
 * We use the "Control Pictures" block for the control characters
 * (0x00-0x1f, 0x7f).
 */
/*static*/ const uint16_t ReformatText::kUTF16Conv[256] = {
    /*0x00*/  0x2400,   // [control] NULL
    /*0x01*/  0x2401,   // [control] START OF HEADING
    /*0x02*/  0x2402,   // [control] START OF TEXT
    /*0x03*/  0x2403,   // [control] END OF TEXT
    /*0x04*/  0x2404,   // [control] END OF TRANSMISSION
    /*0x05*/  0x2405,   // [control] ENQUIRY
    /*0x06*/  0x2406,   // [control] ACKNOWLEDGE
    /*0x07*/  0x2407,   // [control] BELL
    /*0x08*/  0x2408,   // [control] BACKSPACE
    /*0x09*/  0x2409,   // [control] HORIZONTAL TABULATION
    /*0x0a*/  0x240a,   // [control] LINE FEED
    /*0x0b*/  0x240b,   // [control] VERTICAL TABULATION
    /*0x0c*/  0x240c,   // [control] FORM FEED
    /*0x0d*/  0x240d,   // [control] CARRIAGE RETURN
    /*0x0e*/  0x240e,   // [control] SHIFT OUT
    /*0x0f*/  0x240f,   // [control] SHIFT IN
    /*0x10*/  0x2410,   // [control] DATA LINK ESCAPE
    /*0x11*/  0x2411,   // [control] DEVICE CONTROL ONE
    /*0x12*/  0x2412,   // [control] DEVICE CONTROL TWO
    /*0x13*/  0x2413,   // [control] DEVICE CONTROL THREE
    /*0x14*/  0x2414,   // [control] DEVICE CONTROL FOUR
    /*0x15*/  0x2415,   // [control] NEGATIVE ACKNOWLEDGE
    /*0x16*/  0x2416,   // [control] SYNCHRONOUS IDLE
    /*0x17*/  0x2417,   // [control] END OF TRANSMISSION BLOCK
    /*0x18*/  0x2418,   // [control] CANCEL
    /*0x19*/  0x2419,   // [control] END OF MEDIUM
    /*0x1a*/  0x241a,   // [control] SUBSTITUTE
    /*0x1b*/  0x241b,   // [control] ESCAPE
    /*0x1c*/  0x241c,   // [control] FILE SEPARATOR
    /*0x1d*/  0x241d,   // [control] GROUP SEPARATOR
    /*0x1e*/  0x241e,   // [control] RECORD SEPARATOR
    /*0x1f*/  0x241f,   // [control] UNIT SEPARATOR
    /*0x20*/  0x0020,   // SPACE
    /*0x21*/  0x0021,   // EXCLAMATION MARK
    /*0x22*/  0x0022,   // QUOTATION MARK
    /*0x23*/  0x0023,   // NUMBER SIGN
    /*0x24*/  0x0024,   // DOLLAR SIGN
    /*0x25*/  0x0025,   // PERCENT SIGN
    /*0x26*/  0x0026,   // AMPERSAND
    /*0x27*/  0x0027,   // APOSTROPHE
    /*0x28*/  0x0028,   // LEFT PARENTHESIS
    /*0x29*/  0x0029,   // RIGHT PARENTHESIS
    /*0x2A*/  0x002A,   // ASTERISK
    /*0x2B*/  0x002B,   // PLUS SIGN
    /*0x2C*/  0x002C,   // COMMA
    /*0x2D*/  0x002D,   // HYPHEN-MINUS
    /*0x2E*/  0x002E,   // FULL STOP
    /*0x2F*/  0x002F,   // SOLIDUS
    /*0x30*/  0x0030,   // DIGIT ZERO
    /*0x31*/  0x0031,   // DIGIT ONE
    /*0x32*/  0x0032,   // DIGIT TWO
    /*0x33*/  0x0033,   // DIGIT THREE
    /*0x34*/  0x0034,   // DIGIT FOUR
    /*0x35*/  0x0035,   // DIGIT FIVE
    /*0x36*/  0x0036,   // DIGIT SIX
    /*0x37*/  0x0037,   // DIGIT SEVEN
    /*0x38*/  0x0038,   // DIGIT EIGHT
    /*0x39*/  0x0039,   // DIGIT NINE
    /*0x3A*/  0x003A,   // COLON
    /*0x3B*/  0x003B,   // SEMICOLON
    /*0x3C*/  0x003C,   // LESS-THAN SIGN
    /*0x3D*/  0x003D,   // EQUALS SIGN
    /*0x3E*/  0x003E,   // GREATER-THAN SIGN
    /*0x3F*/  0x003F,   // QUESTION MARK
    /*0x40*/  0x0040,   // COMMERCIAL AT
    /*0x41*/  0x0041,   // LATIN CAPITAL LETTER A
    /*0x42*/  0x0042,   // LATIN CAPITAL LETTER B
    /*0x43*/  0x0043,   // LATIN CAPITAL LETTER C
    /*0x44*/  0x0044,   // LATIN CAPITAL LETTER D
    /*0x45*/  0x0045,   // LATIN CAPITAL LETTER E
    /*0x46*/  0x0046,   // LATIN CAPITAL LETTER F
    /*0x47*/  0x0047,   // LATIN CAPITAL LETTER G
    /*0x48*/  0x0048,   // LATIN CAPITAL LETTER H
    /*0x49*/  0x0049,   // LATIN CAPITAL LETTER I
    /*0x4A*/  0x004A,   // LATIN CAPITAL LETTER J
    /*0x4B*/  0x004B,   // LATIN CAPITAL LETTER K
    /*0x4C*/  0x004C,   // LATIN CAPITAL LETTER L
    /*0x4D*/  0x004D,   // LATIN CAPITAL LETTER M
    /*0x4E*/  0x004E,   // LATIN CAPITAL LETTER N
    /*0x4F*/  0x004F,   // LATIN CAPITAL LETTER O
    /*0x50*/  0x0050,   // LATIN CAPITAL LETTER P
    /*0x51*/  0x0051,   // LATIN CAPITAL LETTER Q
    /*0x52*/  0x0052,   // LATIN CAPITAL LETTER R
    /*0x53*/  0x0053,   // LATIN CAPITAL LETTER S
    /*0x54*/  0x0054,   // LATIN CAPITAL LETTER T
    /*0x55*/  0x0055,   // LATIN CAPITAL LETTER U
    /*0x56*/  0x0056,   // LATIN CAPITAL LETTER V
    /*0x57*/  0x0057,   // LATIN CAPITAL LETTER W
    /*0x58*/  0x0058,   // LATIN CAPITAL LETTER X
    /*0x59*/  0x0059,   // LATIN CAPITAL LETTER Y
    /*0x5A*/  0x005A,   // LATIN CAPITAL LETTER Z
    /*0x5B*/  0x005B,   // LEFT SQUARE BRACKET
    /*0x5C*/  0x005C,   // REVERSE SOLIDUS
    /*0x5D*/  0x005D,   // RIGHT SQUARE BRACKET
    /*0x5E*/  0x005E,   // CIRCUMFLEX ACCENT
    /*0x5F*/  0x005F,   // LOW LINE
    /*0x60*/  0x0060,   // GRAVE ACCENT
    /*0x61*/  0x0061,   // LATIN SMALL LETTER A
    /*0x62*/  0x0062,   // LATIN SMALL LETTER B
    /*0x63*/  0x0063,   // LATIN SMALL LETTER C
    /*0x64*/  0x0064,   // LATIN SMALL LETTER D
    /*0x65*/  0x0065,   // LATIN SMALL LETTER E
    /*0x66*/  0x0066,   // LATIN SMALL LETTER F
    /*0x67*/  0x0067,   // LATIN SMALL LETTER G
    /*0x68*/  0x0068,   // LATIN SMALL LETTER H
    /*0x69*/  0x0069,   // LATIN SMALL LETTER I
    /*0x6A*/  0x006A,   // LATIN SMALL LETTER J
    /*0x6B*/  0x006B,   // LATIN SMALL LETTER K
    /*0x6C*/  0x006C,   // LATIN SMALL LETTER L
    /*0x6D*/  0x006D,   // LATIN SMALL LETTER M
    /*0x6E*/  0x006E,   // LATIN SMALL LETTER N
    /*0x6F*/  0x006F,   // LATIN SMALL LETTER O
    /*0x70*/  0x0070,   // LATIN SMALL LETTER P
    /*0x71*/  0x0071,   // LATIN SMALL LETTER Q
    /*0x72*/  0x0072,   // LATIN SMALL LETTER R
    /*0x73*/  0x0073,   // LATIN SMALL LETTER S
    /*0x74*/  0x0074,   // LATIN SMALL LETTER T
    /*0x75*/  0x0075,   // LATIN SMALL LETTER U
    /*0x76*/  0x0076,   // LATIN SMALL LETTER V
    /*0x77*/  0x0077,   // LATIN SMALL LETTER W
    /*0x78*/  0x0078,   // LATIN SMALL LETTER X
    /*0x79*/  0x0079,   // LATIN SMALL LETTER Y
    /*0x7A*/  0x007A,   // LATIN SMALL LETTER Z
    /*0x7B*/  0x007B,   // LEFT CURLY BRACKET
    /*0x7C*/  0x007C,   // VERTICAL LINE
    /*0x7D*/  0x007D,   // RIGHT CURLY BRACKET
    /*0x7E*/  0x007E,   // TILDE
    /*0x7f*/  0x2421,   // [control] DELETE
    /*0x80*/  0x00C4,   // LATIN CAPITAL LETTER A WITH DIAERESIS
    /*0x81*/  0x00C5,   // LATIN CAPITAL LETTER A WITH RING ABOVE
    /*0x82*/  0x00C7,   // LATIN CAPITAL LETTER C WITH CEDILLA
    /*0x83*/  0x00C9,   // LATIN CAPITAL LETTER E WITH ACUTE
    /*0x84*/  0x00D1,   // LATIN CAPITAL LETTER N WITH TILDE
    /*0x85*/  0x00D6,   // LATIN CAPITAL LETTER O WITH DIAERESIS
    /*0x86*/  0x00DC,   // LATIN CAPITAL LETTER U WITH DIAERESIS
    /*0x87*/  0x00E1,   // LATIN SMALL LETTER A WITH ACUTE
    /*0x88*/  0x00E0,   // LATIN SMALL LETTER A WITH GRAVE
    /*0x89*/  0x00E2,   // LATIN SMALL LETTER A WITH CIRCUMFLEX
    /*0x8A*/  0x00E4,   // LATIN SMALL LETTER A WITH DIAERESIS
    /*0x8B*/  0x00E3,   // LATIN SMALL LETTER A WITH TILDE
    /*0x8C*/  0x00E5,   // LATIN SMALL LETTER A WITH RING ABOVE
    /*0x8D*/  0x00E7,   // LATIN SMALL LETTER C WITH CEDILLA
    /*0x8E*/  0x00E9,   // LATIN SMALL LETTER E WITH ACUTE
    /*0x8F*/  0x00E8,   // LATIN SMALL LETTER E WITH GRAVE
    /*0x90*/  0x00EA,   // LATIN SMALL LETTER E WITH CIRCUMFLEX
    /*0x91*/  0x00EB,   // LATIN SMALL LETTER E WITH DIAERESIS
    /*0x92*/  0x00ED,   // LATIN SMALL LETTER I WITH ACUTE
    /*0x93*/  0x00EC,   // LATIN SMALL LETTER I WITH GRAVE
    /*0x94*/  0x00EE,   // LATIN SMALL LETTER I WITH CIRCUMFLEX
    /*0x95*/  0x00EF,   // LATIN SMALL LETTER I WITH DIAERESIS
    /*0x96*/  0x00F1,   // LATIN SMALL LETTER N WITH TILDE
    /*0x97*/  0x00F3,   // LATIN SMALL LETTER O WITH ACUTE
    /*0x98*/  0x00F2,   // LATIN SMALL LETTER O WITH GRAVE
    /*0x99*/  0x00F4,   // LATIN SMALL LETTER O WITH CIRCUMFLEX
    /*0x9A*/  0x00F6,   // LATIN SMALL LETTER O WITH DIAERESIS
    /*0x9B*/  0x00F5,   // LATIN SMALL LETTER O WITH TILDE
    /*0x9C*/  0x00FA,   // LATIN SMALL LETTER U WITH ACUTE
    /*0x9D*/  0x00F9,   // LATIN SMALL LETTER U WITH GRAVE
    /*0x9E*/  0x00FB,   // LATIN SMALL LETTER U WITH CIRCUMFLEX
    /*0x9F*/  0x00FC,   // LATIN SMALL LETTER U WITH DIAERESIS
    /*0xA0*/  0x2020,   // DAGGER
    /*0xA1*/  0x00B0,   // DEGREE SIGN
    /*0xA2*/  0x00A2,   // CENT SIGN
    /*0xA3*/  0x00A3,   // POUND SIGN
    /*0xA4*/  0x00A7,   // SECTION SIGN
    /*0xA5*/  0x2022,   // BULLET
    /*0xA6*/  0x00B6,   // PILCROW SIGN
    /*0xA7*/  0x00DF,   // LATIN SMALL LETTER SHARP S
    /*0xA8*/  0x00AE,   // REGISTERED SIGN
    /*0xA9*/  0x00A9,   // COPYRIGHT SIGN
    /*0xAA*/  0x2122,   // TRADE MARK SIGN
    /*0xAB*/  0x00B4,   // ACUTE ACCENT
    /*0xAC*/  0x00A8,   // DIAERESIS
    /*0xAD*/  0x2260,   // NOT EQUAL TO
    /*0xAE*/  0x00C6,   // LATIN CAPITAL LETTER AE
    /*0xAF*/  0x00D8,   // LATIN CAPITAL LETTER O WITH STROKE
    /*0xB0*/  0x221E,   // INFINITY
    /*0xB1*/  0x00B1,   // PLUS-MINUS SIGN
    /*0xB2*/  0x2264,   // LESS-THAN OR EQUAL TO
    /*0xB3*/  0x2265,   // GREATER-THAN OR EQUAL TO
    /*0xB4*/  0x00A5,   // YEN SIGN
    /*0xB5*/  0x00B5,   // MICRO SIGN
    /*0xB6*/  0x2202,   // PARTIAL DIFFERENTIAL
    /*0xB7*/  0x2211,   // N-ARY SUMMATION
    /*0xB8*/  0x220F,   // N-ARY PRODUCT
    /*0xB9*/  0x03C0,   // GREEK SMALL LETTER PI
    /*0xBA*/  0x222B,   // INTEGRAL
    /*0xBB*/  0x00AA,   // FEMININE ORDINAL INDICATOR
    /*0xBC*/  0x00BA,   // MASCULINE ORDINAL INDICATOR
    /*0xBD*/  0x03A9,   // GREEK CAPITAL LETTER OMEGA
    /*0xBE*/  0x00E6,   // LATIN SMALL LETTER AE
    /*0xBF*/  0x00F8,   // LATIN SMALL LETTER O WITH STROKE
    /*0xC0*/  0x00BF,   // INVERTED QUESTION MARK
    /*0xC1*/  0x00A1,   // INVERTED EXCLAMATION MARK
    /*0xC2*/  0x00AC,   // NOT SIGN
    /*0xC3*/  0x221A,   // SQUARE ROOT
    /*0xC4*/  0x0192,   // LATIN SMALL LETTER F WITH HOOK
    /*0xC5*/  0x2248,   // ALMOST EQUAL TO
    /*0xC6*/  0x2206,   // INCREMENT
    /*0xC7*/  0x00AB,   // LEFT-POINTING DOUBLE ANGLE QUOTATION MARK
    /*0xC8*/  0x00BB,   // RIGHT-POINTING DOUBLE ANGLE QUOTATION MARK
    /*0xC9*/  0x2026,   // HORIZONTAL ELLIPSIS
    /*0xCA*/  0x00A0,   // NO-BREAK SPACE
    /*0xCB*/  0x00C0,   // LATIN CAPITAL LETTER A WITH GRAVE
    /*0xCC*/  0x00C3,   // LATIN CAPITAL LETTER A WITH TILDE
    /*0xCD*/  0x00D5,   // LATIN CAPITAL LETTER O WITH TILDE
    /*0xCE*/  0x0152,   // LATIN CAPITAL LIGATURE OE
    /*0xCF*/  0x0153,   // LATIN SMALL LIGATURE OE
    /*0xD0*/  0x2013,   // EN DASH
    /*0xD1*/  0x2014,   // EM DASH
    /*0xD2*/  0x201C,   // LEFT DOUBLE QUOTATION MARK
    /*0xD3*/  0x201D,   // RIGHT DOUBLE QUOTATION MARK
    /*0xD4*/  0x2018,   // LEFT SINGLE QUOTATION MARK
    /*0xD5*/  0x2019,   // RIGHT SINGLE QUOTATION MARK
    /*0xD6*/  0x00F7,   // DIVISION SIGN
    /*0xD7*/  0x25CA,   // LOZENGE
    /*0xD8*/  0x00FF,   // LATIN SMALL LETTER Y WITH DIAERESIS
    /*0xD9*/  0x0178,   // LATIN CAPITAL LETTER Y WITH DIAERESIS
    /*0xDA*/  0x2044,   // FRACTION SLASH
    /*0xDB*/  0x00A4,   // CURRENCY SIGN (was EURO SIGN)
    /*0xDC*/  0x2039,   // SINGLE LEFT-POINTING ANGLE QUOTATION MARK
    /*0xDD*/  0x203A,   // SINGLE RIGHT-POINTING ANGLE QUOTATION MARK
    /*0xDE*/  0xFB01,   // LATIN SMALL LIGATURE FI
    /*0xDF*/  0xFB02,   // LATIN SMALL LIGATURE FL
    /*0xE0*/  0x2021,   // DOUBLE DAGGER
    /*0xE1*/  0x00B7,   // MIDDLE DOT
    /*0xE2*/  0x201A,   // SINGLE LOW-9 QUOTATION MARK
    /*0xE3*/  0x201E,   // DOUBLE LOW-9 QUOTATION MARK
    /*0xE4*/  0x2030,   // PER MILLE SIGN
    /*0xE5*/  0x00C2,   // LATIN CAPITAL LETTER A WITH CIRCUMFLEX
    /*0xE6*/  0x00CA,   // LATIN CAPITAL LETTER E WITH CIRCUMFLEX
    /*0xE7*/  0x00C1,   // LATIN CAPITAL LETTER A WITH ACUTE
    /*0xE8*/  0x00CB,   // LATIN CAPITAL LETTER E WITH DIAERESIS
    /*0xE9*/  0x00C8,   // LATIN CAPITAL LETTER E WITH GRAVE
    /*0xEA*/  0x00CD,   // LATIN CAPITAL LETTER I WITH ACUTE
    /*0xEB*/  0x00CE,   // LATIN CAPITAL LETTER I WITH CIRCUMFLEX
    /*0xEC*/  0x00CF,   // LATIN CAPITAL LETTER I WITH DIAERESIS
    /*0xED*/  0x00CC,   // LATIN CAPITAL LETTER I WITH GRAVE
    /*0xEE*/  0x00D3,   // LATIN CAPITAL LETTER O WITH ACUTE
    /*0xEF*/  0x00D4,   // LATIN CAPITAL LETTER O WITH CIRCUMFLEX
    /*0xF0*/  0xF8FF,   // Apple logo
    /*0xF1*/  0x00D2,   // LATIN CAPITAL LETTER O WITH GRAVE
    /*0xF2*/  0x00DA,   // LATIN CAPITAL LETTER U WITH ACUTE
    /*0xF3*/  0x00DB,   // LATIN CAPITAL LETTER U WITH CIRCUMFLEX
    /*0xF4*/  0x00D9,   // LATIN CAPITAL LETTER U WITH GRAVE
    /*0xF5*/  0x0131,   // LATIN SMALL LETTER DOTLESS I
    /*0xF6*/  0x02C6,   // MODIFIER LETTER CIRCUMFLEX ACCENT
    /*0xF7*/  0x02DC,   // SMALL TILDE
    /*0xF8*/  0x00AF,   // MACRON
    /*0xF9*/  0x02D8,   // BREVE
    /*0xFA*/  0x02D9,   // DOT ABOVE
    /*0xFB*/  0x02DA,   // RING ABOVE
    /*0xFC*/  0x00B8,   // CEDILLA
    /*0xFD*/  0x02DD,   // DOUBLE ACUTE ACCENT
    /*0xFE*/  0x02DB,   // OGONEK
    /*0xFF*/  0x02C7,   // CARON
};

/*
 * Quick sanity check on contents of array.
 *
 * No two characters should map to the same thing.  This isn't vital, but
 * if we want to have a reversible transformation someday, it'll make our
 * lives easier then.
 */
void ReformatText::CheckGSCharConv(void)
{
#ifdef _DEBUG
    bool* test = (bool*) malloc(65536 * sizeof(bool));

    memset(test, 0, 65536 * sizeof(bool));
    for (int i = 0; i < NELEM(kCP1252Conv); i++) {
        if (test[kCP1252Conv[i]] && kCP1252Conv[i] != kUnk) {
            LOGW("Character used twice: 0x%02x at %d (0x%02x)",
                kCP1252Conv[i], i, i+128);
            assert(false);
        }
        test[kCP1252Conv[i]] = true;
    }

    memset(test, 0, 65536 * sizeof(bool));
    for (int i = 0; i < NELEM(kUTF16Conv); i++) {
        if (test[kUTF16Conv[i]]) {
            LOGW("Character used twice: 0x%02x at %d (0x%02x)",
                kUTF16Conv[i], i, i+128);
            assert(false);
        }
        test[kUTF16Conv[i]] = true;
    }

    free(test);
#endif
}

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

void ReformatText::RTFSetColor(TextColor color)
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
        LOGI("Unrecognized font family 0x%04x", family);
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
void ReformatText::RTFSetGSFontStyle(uint8_t qdStyle)
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
 *2
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
 * Returns 0 on success, nonzero if the buffer is overfilled or underfilled.
 */
int ReformatGraphics::UnpackBytes(uint8_t* dst, const uint8_t* src,
    long dstRem, long srcLen)
{
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

    /* require that we completely fill the buffer */
    if (dstRem != 0) {
        LOGI(" SHR unpack dstRem at %d", dstRem);
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
