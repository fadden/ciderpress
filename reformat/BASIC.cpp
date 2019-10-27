/*
 * CiderPress
 * Copyright (C) 2007, 2008 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Convert BASIC programs.
 */
#include "StdAfx.h"
#include "BASIC.h"


/* our color map */
const ReformatText::TextColor kDefaultColor = ReformatText::kColorDarkGrey;
const ReformatText::TextColor kLineNumColor = ReformatText::kColorDarkGrey;
const ReformatText::TextColor kKeywordColor = ReformatText::kColorBlack;
const ReformatText::TextColor kCommentColor = ReformatText::kColorMediumGreen;
const ReformatText::TextColor kStringColor  = ReformatText::kColorMediumBlue;
const ReformatText::TextColor kColonColor   = ReformatText::kColorRed;
 //kColorMediumGrey;

/*
 * ===========================================================================
 *      Applesoft BASIC
 * ===========================================================================
 */

/*
 * Applesoft BASIC file format:
 *
 *  <16-bit file length>  [DOS 3.3 only; not visible here]
 *  <line> ...
 *  <EOF marker ($0000)>
 *
 * Each line consists of:
 *  <16-bit address of next line (relative to $800)>
 *  <16-bit line number, usually 0-63999>
 *  <tokens | characters> ...
 *  <EOL marker ($00)>
 *
 * All values are little-endian.  Numbers are stored as characters.
 */

/*
 * Decide whether or not we want to handle this file.
 */
void ReformatApplesoft::Examine(ReformatHolder* pHolder)
{
    ReformatHolder::ReformatApplies applies = ReformatHolder::kApplicNot;

    if (pHolder->GetFileType() == kTypeBAS)
        applies = ReformatHolder::kApplicProbably;

    pHolder->SetApplic(ReformatHolder::kReformatApplesoft, applies,
        ReformatHolder::kApplicNot, ReformatHolder::kApplicNot);
    pHolder->SetApplic(ReformatHolder::kReformatApplesoft_Hilite, applies,
        ReformatHolder::kApplicNot, ReformatHolder::kApplicNot);

    if (pHolder->GetOption(ReformatHolder::kOptHiliteBASIC) != 0)
        pHolder->SetApplicPreferred(ReformatHolder::kReformatApplesoft_Hilite);
    else
        pHolder->SetApplicPreferred(ReformatHolder::kReformatApplesoft);
}

/*
 * Values from 128 to 234 are tokens in Applesoft.  Values from 235 to 255
 * show up as error messages.  The goal here is to produce values that are
 * human-readable and/or EXECable, so no attempt has been made to display
 * the error values.
 */
static const char gApplesoftTokens[128 * ReformatApplesoft::kTokenLen] = {
    "END\0    FOR\0    NEXT\0   DATA\0   INPUT\0  DEL\0    DIM\0    READ\0   "
    "GR\0     TEXT\0   PR#\0    IN#\0    CALL\0   PLOT\0   HLIN\0   VLIN\0   "
    "HGR2\0   HGR\0    HCOLOR=\0HPLOT\0  DRAW\0   XDRAW\0  HTAB\0   HOME\0   "
    "ROT=\0   SCALE=\0 SHLOAD\0 TRACE\0  NOTRACE\0NORMAL\0 INVERSE\0FLASH\0  "
    "COLOR=\0 POP\0    VTAB\0   HIMEM:\0 LOMEM:\0 ONERR\0  RESUME\0 RECALL\0 "
    "STORE\0  SPEED=\0 LET\0    GOTO\0   RUN\0    IF\0     RESTORE\0&\0      "
    "GOSUB\0  RETURN\0 REM\0    STOP\0   ON\0     WAIT\0   LOAD\0   SAVE\0   "
    "DEF\0    POKE\0   PRINT\0  CONT\0   LIST\0   CLEAR\0  GET\0    NEW\0    "
    "TAB(\0   TO\0     FN\0     SPC(\0   THEN\0   AT\0     NOT\0    STEP\0   "
    "+\0      -\0      *\0      /\0      ^\0      AND\0    OR\0     >\0      "
    "=\0      <\0      SGN\0    INT\0    ABS\0    USR\0    FRE\0    SCRN(\0  "
    "PDL\0    POS\0    SQR\0    RND\0    LOG\0    EXP\0    COS\0    SIN\0    "
    "TAN\0    ATN\0    PEEK\0   LEN\0    STR$\0   VAL\0    ASC\0    CHR$\0   "
    "LEFT$\0  RIGHT$\0 MID$\0   ERROR\0  ERROR\0  ERROR\0  ERROR\0  ERROR\0  "
    "ERROR\0  ERROR\0  ERROR\0  ERROR\0  ERROR\0  ERROR\0  ERROR\0  ERROR\0  "
    "ERROR\0  ERROR\0  ERROR\0  ERROR\0  ERROR\0  ERROR\0  ERROR\0  ERROR\0 "
};

/*
 * Make table available.
 */
/*static*/ const char* ReformatApplesoft::GetApplesoftTokens(void)
{
    return gApplesoftTokens;
}


/*
 * Reformat an Applesoft BASIC program into a text format that mimics the
 * output of the "LIST" command (with POKE 33,73 to suppress CRs).
 */
int ReformatApplesoft::Process(const ReformatHolder* pHolder,
    ReformatHolder::ReformatID id, ReformatHolder::ReformatPart part,
    ReformatOutput* pOutput)
{
    const uint8_t* srcPtr = pHolder->GetSourceBuf(part);
    long srcLen = pHolder->GetSourceLen(part);
    long length = srcLen;
    int retval = -1;

    if (srcLen > 65536)
        fUseRTF = false;
    if (fUseRTF) {
        if (id != ReformatHolder::kReformatApplesoft_Hilite)
            fUseRTF = false;
    }

    RTFBegin(kRTFFlagColorTable);

    /*
     * Make sure there's enough here to get started.  We want to return an
     * "okay" result because we want this treated like a reformatted empty
     * BASIC program rather than a non-Applesoft file.
     */
    if (length < 2) {
        LOGI("  BAS truncated?");
        //fExpBuf.CreateWorkBuf();
        BufPrintf("\r\n");
        goto done;
    }

    while (length > 0) {
        uint16_t nextAddr;
        uint16_t lineNum;
        bool inQuote = false;
        bool inRem = false;

        nextAddr = Read16(&srcPtr, &length);
        if (nextAddr == 0) {
            /* ProDOS sticks an extra byte on the end? */
            if (length > 1) {
                LOGI("  BAS ended early; len is %d", length);
            }
            break;
        }

        /* print line number */
        RTFSetColor(kLineNumColor);
        lineNum = Read16(&srcPtr, &length);
        BufPrintf(" %u ", lineNum);

        RTFSetColor(kDefaultColor);

        assert(kTokenLen == 8);     // we do "<< 3" below, so this must hold

        /* print a line */
        while (*srcPtr != 0 && length > 0) {
            if (*srcPtr & 0x80) {
                /* token */
                //RTFBoldOn();
                RTFSetColor(kKeywordColor);
                BufPrintf(" %s ", &gApplesoftTokens[((*srcPtr) & 0x7f) << 3]);
                //RTFBoldOff();
                RTFSetColor(kDefaultColor);

                if (*srcPtr == 0xb2) {
                    // REM -- do rest of line in green
                    RTFSetColor(kCommentColor);
                    inRem = true;
                }
            } else {
                /* simple character */
                if (fUseRTF) {
                    if (*srcPtr == '"' && !inRem) {
                        if (!inQuote) {
                            RTFSetColor(kStringColor);
                            RTFPrintChar(*srcPtr);
                        } else {
                            RTFPrintChar(*srcPtr);
                            RTFSetColor(kDefaultColor);
                        }
                        inQuote = !inQuote;
                    } else if (*srcPtr == ':' && !inRem && !inQuote) {
                        RTFSetColor(kColonColor);
                        RTFPrintChar(*srcPtr);
                        RTFSetColor(kDefaultColor);
                    } else if (inRem && *srcPtr == '\r') {
                        RTFNewPara();
                    } else {
                        RTFPrintChar(*srcPtr);
                    }
                } else {
                    if (inRem && *srcPtr == '\r') {
                        BufPrintf("\r\n");
                    } else {
                        BufPrintf("%c", *srcPtr);
                    }
                }
            }

            srcPtr++;
            length--;
        }

        if (inQuote || inRem)
            RTFSetColor(kDefaultColor);
        inQuote = inRem = false;

        srcPtr++;
        length--;

        if (!length) {
            LOGI("  BAS truncated in mid-line");
            break;
        }

        RTFNewPara();
    }

done:
    RTFEnd();

    SetResultBuffer(pOutput);
    retval = 0;

//bail:
    return retval;
}


/*
 * ===========================================================================
 *      Integer BASIC
 * ===========================================================================
 */
#include "Asm.h"

/*
 * Integer BASIC file format (thanks to Paul Schlyter, pausch at saaf.se):
 *
 *  <16-bit file length>  [DOS 3.3 only; not visible here]
 *  <line> ...
 *
 * Each line consists of:
 *  <8-bit line length>
 *  <16-bit line number>
 *  <token | character | variable> ...
 *  <end-of-line token ($01)>
 *
 * Each line is a stream of bytes:
 *  $01: end of line
 *  $12-$7f: language token
 *  $b0-b9 ('0'-'9'): start of integer constant (first byte has no meaning?)
 *          next 16 bits hold the number
 *  $c1-da ('A'-'Z'): start of a variable name; ends on value <0x80
 *          next several bytes hold the name
 *
 * Most of the first $11 tokens are illegal except as part of an integer
 * constant, which just means that you can't type them into a program from
 * the keyboard.  If you POKE them in manually, things like "himem:" and
 * "del" will work.
 *
 * $ba-$c0 and $db-$ff are illegal except in a string constant.
 *
 * There is no end-of-file marker.
 */

/*
 * Tokens.
 */
static const char* const gIntegerTokens[128] = {
    "HIMEM:",   "<EOL>",    "_ ",       ":",
    "LOAD ",    "SAVE ",    "CON ",     "RUN ",
    "RUN ",     "DEL ",     ",",        "NEW ",
    "CLR ",     "AUTO ",    ",",        "MAN ",
    "HIMEM:",   "LOMEM:",   "+",        "-",
    "*",        "/",        "=",        "#",
    ">=",       ">",        "<=",       "<>",
    "<",        "AND ",     "OR ",      "MOD ",

    "^ ",       "+",        "(",        ",",
    "THEN ",    "THEN ",    ",",        ",",
    "\"",       "\"",       "(",        "!",
    "!",        "(",        "PEEK ",    "RND ",
    "SGN ",     "ABS ",     "PDL ",     "RNDX ",
    "(",        "+",        "-",        "NOT ",
    "(",        "=",        "#",        "LEN(",
    "ASC( ",    "SCRN( ",   ",",        "(",

    "$",        "$",        "(",        ",",
    ",",        ";",        ";",        ";",
    ",",        ",",        ",",        "TEXT ",
    "GR ",      "CALL ",    "DIM ",     "DIM ",
    "TAB ",     "END ",     "INPUT ",   "INPUT ",
    "INPUT ",   "FOR ",     "=",        "TO ",
    "STEP ",    "NEXT ",    ",",        "RETURN ",
    "GOSUB ",   "REM ",     "LET ",     "GOTO ",

    "IF ",      "PRINT ",   "PRINT ",   "PRINT ",
    "POKE ",    ",",        "COLOR= ",  "PLOT ",
    ",",        "HLIN ",    ",",        "AT ",
    "VLIN ",    ",",        "AT ",      "VTAB ",
    "=",        "=",        ")",        ")",
    "LIST ",    ",",        "LIST ",    "POP ",
    "NODSP ",   "NODSP ",   "NOTRACE ", "DSP ",
    "DSP ",     "TRACE ",   "PR# ",     "IN# "
};

/*
 * Decide whether or not we want to handle this file.
 */
void ReformatInteger::Examine(ReformatHolder* pHolder)
{
    ReformatHolder::ReformatApplies apply = ReformatHolder::kApplicNot;
    if (pHolder->GetFileType() == kTypeINT) {
        if (ReformatSCAssem::IsSCAssem(pHolder)) {
            /* possibly intbasic */
            apply = ReformatHolder::kApplicMaybe;
        } else if (ReformatLISA3::IsLISA(pHolder)) {
            /* possibly intbasic */
            apply = ReformatHolder::kApplicMaybe;
        } else if (ReformatLISA4::IsLISA(pHolder)) {
            /* possibly intbasic */
            apply = ReformatHolder::kApplicMaybe;
        } else {
            /* definitely intbasic */
            apply = ReformatHolder::kApplicYes;
        }
    } else {
        /* not intbasic */
        apply = ReformatHolder::kApplicNot;
    }

    //apply = ReformatHolder::kApplicMaybe;     // DEBUG DEBUG

    pHolder->SetApplic(ReformatHolder::kReformatInteger, apply,
        ReformatHolder::kApplicNot, ReformatHolder::kApplicNot);
    pHolder->SetApplic(ReformatHolder::kReformatInteger_Hilite, apply,
        ReformatHolder::kApplicNot, ReformatHolder::kApplicNot);

    if (pHolder->GetOption(ReformatHolder::kOptHiliteBASIC) != 0)
        pHolder->SetApplicPreferred(ReformatHolder::kReformatInteger_Hilite);
    else
        pHolder->SetApplicPreferred(ReformatHolder::kReformatInteger);
}

/*
 * Reformat an Integer BASIC program into a text format that mimics the
 * output of the "LIST" command.
 */
int ReformatInteger::Process(const ReformatHolder* pHolder,
    ReformatHolder::ReformatID id, ReformatHolder::ReformatPart part,
    ReformatOutput* pOutput)
{
    const uint8_t* srcPtr = pHolder->GetSourceBuf(part);
    long srcLen = pHolder->GetSourceLen(part);
    long length = srcLen;
    int retval = -1;

    //srcPtr += 0xff0; //0x228e;
    //srcLen -= 0xff0; //0x228e;

    if (srcLen > 65536)
        fUseRTF = false;
    if (fUseRTF) {
        if (id != ReformatHolder::kReformatInteger_Hilite)
            fUseRTF = false;
    }

    RTFBegin(kRTFFlagColorTable);

    /*
     * Make sure there's enough here to get started.  We want to return an
     * "okay" result because we want this treated like a reformatted empty
     * BASIC program rather than a non-Integer file.
     */
    if (length < 2) {
        LOGI("  INT truncated?");
        BufPrintf("\r\n");
        goto done;
    }

    while (length > 0) {
        uint8_t lineLen;
        uint16_t lineNum;
        bool trailingSpace;
        bool newTrailingSpace = false;

        /* pull the length byte, which we sanity-check */
        lineLen = *srcPtr++;
        length--;
        if (lineLen == 0) {
            LOGI("  INT found zero-length line?");
            break;
        }

        /* line number */
        RTFSetColor(kLineNumColor);
        lineNum = Read16(&srcPtr, &length);
        BufPrintf("%5u ", lineNum);
        RTFSetColor(kDefaultColor);

        trailingSpace = true;
        while (*srcPtr != 0x01 && length > 0) {
            if (*srcPtr == 0x28) {
                /* start of quoted text */
                RTFSetColor(kStringColor);
                BufPrintf("\"");
                length--;
                while (*++srcPtr != 0x29 && length > 0) {
                    /* escape chars, but let Ctrl-D and Ctrl-G through */
                    if (fUseRTF && *srcPtr != 0x84 && *srcPtr != 0x87)
                        RTFPrintChar(*srcPtr & 0x7f);
                    else
                        BufPrintf("%c", *srcPtr & 0x7f);
                    length--;
                }
                if (*srcPtr != 0x29) {
                    LOGI("  INT ended while in a string constant");
                    break;
                }
                BufPrintf("\"");
                RTFSetColor(kDefaultColor);
                srcPtr++;
                length--;
            } else if (*srcPtr == 0x5d) {
                /* start of REM statement, run to EOL */
                //RTFBoldOn();
                RTFSetColor(kKeywordColor);
                BufPrintf("%sREM ", trailingSpace ? "" : " ");
                //RTFBoldOff();
                RTFSetColor(kCommentColor);
                length--;
                while (*++srcPtr != 0x01) {
                    if (fUseRTF)
                        RTFPrintChar(*srcPtr & 0x7f);
                    else
                        BufPrintf("%c", *srcPtr & 0x7f);
                    length--;
                }
                RTFSetColor(kDefaultColor);
                if (*srcPtr != 0x01) {
                    LOGI("  INT ended while in a REM statement");
                    break;
                }
            } else if (*srcPtr >= 0xb0 && *srcPtr <= 0xb9) {
                /* start of integer constant */
                srcPtr++;
                length--;
                if (length < 2) {
                    LOGI("  INT ended while in an integer constant");
                    break;
                }
                int val;
                val = Read16(&srcPtr, &length);
                BufPrintf("%d", val);
            } else if (*srcPtr >= 0xc1 && *srcPtr <= 0xda) {
                /* start of variable name */
                while ((*srcPtr >= 0xc1 && *srcPtr <= 0xda) ||
                       (*srcPtr >= 0xb0 && *srcPtr <= 0xb9))
                {
                    /* note no RTF-escaped chars in this range */
                    BufPrintf("%c", *srcPtr & 0x7f);
                    srcPtr++;
                    length--;
                }
            } else if (*srcPtr < 0x80) {
                /* found a token; try to get the whitespace right */
                /* (maybe should've left whitespace on the ends of tokens
                    that are always followed by whitespace...?) */
                const char* token;
                token = gIntegerTokens[*srcPtr];
                //RTFBoldOn();
                if (*srcPtr == 0x03)    // colon
                    RTFSetColor(kColonColor);
                else
                    RTFSetColor(kKeywordColor);
                if (token[0] >= 0x21 && token[0] <= 0x3f || *srcPtr < 0x12) {
                    /* does not need leading space */
                    BufPrintf("%s", token);
                } else {
                    /* needs leading space; combine with prev if it exists */
                    if (trailingSpace)
                        BufPrintf("%s", token);
                    else
                        BufPrintf(" %s", token);
                }
                if (token[strlen(token)-1] == ' ')
                    newTrailingSpace = true;
                //RTFBoldOff();
                RTFSetColor(kDefaultColor);
                srcPtr++;
                length--;
            } else {
                /* should not happen */
                LOGI("  INT unexpected value 0x%02x at byte %d",
                    *srcPtr, srcPtr - pHolder->GetSourceBuf(part));

                /* skip past it and keep trying */
                srcPtr++;
                length--;
            }

            trailingSpace = newTrailingSpace;
            newTrailingSpace = false;
        } /*while line*/

        /* skip past EOL token */
        if (*srcPtr != 0x01 && length > 0) {
            LOGI("bailing");     // must've failed during processing
            goto bail;
        }
        srcPtr++;
        length--;

        RTFNewPara();
    }

done:
    RTFEnd();

    SetResultBuffer(pOutput);
    retval = 0;

bail:
    return retval;
}

/*
 * ===========================================================================
 *      Apple /// Business BASIC
 * ===========================================================================
 */

/*
 * Apple /// Business BASIC file format:
 *
 *  <16-bit file length>
 *  <line> ...
 *  <EOF marker ($0000)>
 *
 * Each line consists of:
 *  <8-bit offset to next line>
 *  <16-bit line number, usually 0-63999>
 *  <tokens | characters> ...
 *  <EOL marker ($00)>
 *
 * All values are little-endian.  Numbers are stored as characters.
 */

/*
 * Decide whether or not we want to handle this file.
 */
void ReformatBusiness::Examine(ReformatHolder* pHolder)
{
    ReformatHolder::ReformatApplies applies = ReformatHolder::kApplicNot;

    if (pHolder->GetFileType() == kTypeBA3)
        applies = ReformatHolder::kApplicYes;

    pHolder->SetApplic(ReformatHolder::kReformatBusiness, applies,
        ReformatHolder::kApplicNot, ReformatHolder::kApplicNot);
    pHolder->SetApplic(ReformatHolder::kReformatBusiness_Hilite, applies,
        ReformatHolder::kApplicNot, ReformatHolder::kApplicNot);

    if (pHolder->GetOption(ReformatHolder::kOptHiliteBASIC) != 0)
        pHolder->SetApplicPreferred(ReformatHolder::kReformatBusiness_Hilite);
    else
        pHolder->SetApplicPreferred(ReformatHolder::kReformatBusiness);
}

/*
 * Values from 128 to 234 are tokens in Business BASIC.  Values from 235 to 255
 * show up as error messages.  The goal here is to produce values that are
 * human-readable and/or EXECable, so no attempt has been made to display
 * the error values.
 * TODO: verify this comment -- looks like copy & paste from BAS token table
 */
static const char gBusinessTokens[128*10] = {
/* 0x80 */ "END\0      FOR\0      NEXT\0     INPUT\0    OUTPUT\0   DIM\0      READ\0     WRITE\0    "
/* 0x88 */ "OPEN\0     CLOSE\0    *error*\0  TEXT\0     *error*\0  BYE\0      *error*\0  *error*\0  "
/* 0x90 */ "*error*\0  *error*\0  *error*\0  WINDOW\0   INVOKE\0   PERFORM\0  *error*\0  *error*\0  "
/* 0x98 */ "FRE\0      HPOS\0     VPOS\0     ERRLIN\0   ERR\0      KBD\0      EOF\0      TIME$\0    "
/* 0xa0 */ "DATE$\0    PREFIX$\0  EXFN.\0    EXFN%.\0   OUTREC\0   INDENT\0   *error*\0  *error*\0  "
/* 0xa8 */ "*error*\0  *error*\0  *error*\0  *error*\0  *error*\0  POP\0      HOME\0     *error*\0  "
/* 0xb0 */ "SUB$(\0    OFF\0      TRACE\0    NOTRACE\0  NORMAL\0   INVERSE\0  SCALE(\0   RESUME\0   "
/* 0xb8 */ "*error*\0  LET\0      GOTO\0     IF\0       RESTORE\0  SWAP\0     GOSUB\0    RETURN\0   "
/* 0xc0 */ "REM\0      STOP\0     ON\0       *error*\0  LOAD\0     SAVE\0     DELETE\0   RUN\0      "
/* 0xc8 */ "RENAME\0   LOCK\0     UNLOCK\0   CREATE\0   EXEC\0     CHAIN\0    *error*\0  *error*\0  "
/* 0xd0 */ "*error*\0  CATALOG\0  *error*\0  *error*\0  DATA\0     IMAGE\0    CAT\0      DEF\0      "
/* 0xd8 */ "*error*\0  PRINT\0    DEL\0      ELSE\0     CONT\0     LIST\0     CLEAR\0    GET\0      "
/* 0xe0 */ "NEW\0      TAB\0      TO\0       SPC(\0     USING\0    THEN\0     *error*\0  MOD\0      "
/* 0xe8 */ "STEP\0     AND\0      OR\0       EXTENSION\0DIV\0      *error*\0  FN\0       NOT\0      "
/* 0xf0 */ "*error*\0  *error*\0  *error*\0  *error*\0  *error*\0  *error*\0  *error*\0  *error*\0  "
/* 0xf8 */ "*error*\0  *error*\0  *error*\0  *error*\0  *error*\0  *error*\0  *error*\0  tf7\0     "
};

static const char gExtendedBusinessTokens[128*10] = {
/* 0x80 */ "TAB(\0     TO\0       SPC(\0     USING\0    THEN\0     *error*\0  MOD\0      STEP\0     "
/* 0x88 */ "AND\0      OR\0       EXTENSION\0DIV\0      *error*\0  FN\0       NOT\0      *error*\0  "
/* 0x90 */ "*error*\0  *error*\0  *error*\0  *error*\0  *error*\0  *error*\0  *error*\0  *error*\0  "
/* 0x98 */ "*error*\0  *error*\0  *error*\0  *error*\0  AS\0       SGN(\0     INT(\0     ABS(\0     "
/* 0xa0 */ "*error*\0  TYP(\0     REC(\0     *error*\0  *error*\0  *error*\0  *error*\0  *error*\0  "
/* 0xa8 */ "*error*\0  *error*\0  *error*\0  *error*\0  *error*\0  PDL(\0     BUTTON(\0  SQR(\0     "
/* 0xb0 */ "RND(\0     LOG(\0     EXP(\0     COS(\0     SIN(\0     TAN(\0     ATN(\0     *error*\0  "
/* 0xb8 */ "*error*\0  *error*\0  *error*\0  *error*\0  *error*\0  *error*\0  *error*\0  *error*\0  "
/* 0xc0 */ "*error*\0  *error*\0  *error*\0  STR$(\0    HEX$(\0    CHR$(\0    LEN(\0     VAL(\0     "
/* 0xc8 */ "ASC(\0     TEN(\0     *error*\0  *error*\0  CONV(\0    CONV&(\0   CONV$(\0   CONV%(\0   "
/* 0xd0 */ "LEFT$(\0   RIGHT$(\0  MID$(\0    INSTR(\0   *error*\0  *error*\0  *error*\0  *error*\0  "
/* 0xd8 */ "*error*\0  *error*\0  *error*\0  *error*\0  *error*\0  *error*\0  *error*\0  *error*\0  "
/* 0xe0 */ "*error*\0  *error*\0  *error*\0  *error*\0  *error*\0  *error*\0  *error*\0  *error*\0  "
/* 0xe8 */ "*error*\0  *error*\0  *error*\0  *error*\0  *error*\0  *error*\0  *error*\0  *error*\0  "
/* 0xf0 */ "*error*\0  *error*\0  *error*\0  *error*\0  *error*\0  *error*\0  *error*\0  *error*\0  "
/* 0xf8 */ "*error*\0  *error*\0  *error*\0  *error*\0  *error*\0  *error*\0  *error*\0  *error*\0 "
};

/*
 * Reformat an Apple /// Business BASIC program into a text format that
 * mimics the output of the "LIST" command.
 */
int ReformatBusiness::Process(const ReformatHolder* pHolder,
    ReformatHolder::ReformatID id, ReformatHolder::ReformatPart part,
    ReformatOutput* pOutput)
{
    const uint8_t* srcPtr = pHolder->GetSourceBuf(part);
    long srcLen = pHolder->GetSourceLen(part);
    long length = srcLen;
    int retval = -1;
    int nestLevels = 0;

    if (srcLen > 65536)
        fUseRTF = false;
    if (fUseRTF) {
        if (id != ReformatHolder::kReformatBusiness_Hilite)
            fUseRTF = false;
    }

    RTFBegin(kRTFFlagColorTable);

    /*
     * Make sure there's enough here to get started.  We want to return an
     * "okay" result because we want this treated like a reformatted empty
     * BASIC program rather than a non-BASIC file.
     */
    if (length < 2) {
        LOGI("  BA3 truncated?");
        //fExpBuf.CreateWorkBuf();
        BufPrintf("\r\n");
        goto done;
    }

    uint16_t fileLength;
    fileLength = Read16(&srcPtr, &length);
    LOGI("  BA3 internal file length is: %d", fileLength);

    while (length > 0) {
        uint16_t increment;
        uint16_t extendedToken;
        uint16_t lineNum;
        bool inQuote = false;
        bool inRem = false;
        bool firstData = true;
        bool literalYet = false;

        increment = Read8(&srcPtr, &length);
        LOGI("  BA3 increment to next line is: %d", increment);
        if (increment == 0) {
            /* ProDOS sticks an extra byte on the end? */
            if (length > 1) {
                LOGI("  BA3 ended early; len is %d", length);
            }
            break;
        }

        /* print line number */
        RTFSetColor(kLineNumColor);
        lineNum = Read16(&srcPtr, &length);
        LOGI("  BA3 line number: %d", lineNum);
        BufPrintf(" %u   ", lineNum);

        RTFSetColor(kDefaultColor);
        if (nestLevels > 0) {
            for (int i =0; i < nestLevels; i++)
                BufPrintf("  ");
        }

        /* print a line */
        while (*srcPtr != 0 && length > 0) {
            if (*srcPtr & 0x80) {
                /* token */
                //RTFBoldOn();
                literalYet = false;
                RTFSetColor(kKeywordColor);
                if (*srcPtr == 0x81) {
                    // Token is FOR - indent
                    nestLevels ++;
                }
                else if (*srcPtr == 0x82) {
                    // Token is NEXT - outdent
                    nestLevels --;
                }
                if (!firstData)
                    BufPrintf(" ");
                if (((*srcPtr) & 0x7f) == 0x7f)
                {
                    extendedToken = Read8(&srcPtr, &length);
                    BufPrintf("%s", &gExtendedBusinessTokens[((*srcPtr) & 0x7f) * 10]);
                    // We need to have some tokens NOT add a space after them.
                    if ((*srcPtr == 0x80) ||    // TAB(
                        (*srcPtr == 0x82) ||    // SPC(
                        (*srcPtr == 0x9d) ||    // SGN(
                        (*srcPtr == 0x9e) ||    // INT(
                        (*srcPtr == 0x9f) ||    // ABS(
                        (*srcPtr == 0xa1) ||    // TYP(
                        (*srcPtr == 0xa2) ||    // REC(
                        (*srcPtr == 0xad) ||    // PDL(
                        (*srcPtr == 0xae) ||    // BUTTON(
                        (*srcPtr == 0xaf) ||    // SQR(
                        (*srcPtr == 0xb0) ||    // RND(
                        (*srcPtr == 0xb1) ||    // LOG(
                        (*srcPtr == 0xb2) ||    // EXP(
                        (*srcPtr == 0xb3) ||    // COS(
                        (*srcPtr == 0xb4) ||    // SIN(
                        (*srcPtr == 0xb5) ||    // TAN(
                        (*srcPtr == 0xb6) ||    // ATN(
                        (*srcPtr == 0xc3) ||    // STR$(
                        (*srcPtr == 0xc4) ||    // HEX$(
                        (*srcPtr == 0xc5) ||    // CHR$(
                        (*srcPtr == 0xc6) ||    // LEN(
                        (*srcPtr == 0xc7) ||    // VAL(
                        (*srcPtr == 0xc8) ||    // ASC(
                        (*srcPtr == 0xc9) ||    // TEN(
                        (*srcPtr == 0xcc) ||    // CONV(
                        (*srcPtr == 0xcd) ||    // CONV&(
                        (*srcPtr == 0xce) ||    // CONV$(
                        (*srcPtr == 0xcf) ||    // CONV%(
                        (*srcPtr == 0xd0) ||    // LEFT$(
                        (*srcPtr == 0xd1) ||    // RIGHT$(
                        (*srcPtr == 0xd2) ||    // MID$(
                        (*srcPtr == 0xd3))      // INSTR(
                        firstData = true;
                    else
                        firstData = false;
                }
                else {
                    BufPrintf("%s", &gBusinessTokens[((*srcPtr) & 0x7f) * 10]);
                    // We need to have some tokens NOT add a space after them.
                    if ((*srcPtr == 0x99) ||    // HPOS
                        (*srcPtr == 0x9a) ||    // VPOS
                        (*srcPtr == 0x9f) ||    // TIME$
                        (*srcPtr == 0xa0) ||    // DATE$
                        (*srcPtr == 0xa1) ||    // PREFIX$
                        (*srcPtr == 0xa2) ||    // EXFN.
                        (*srcPtr == 0xa3) ||    // EXFN%.
                        (*srcPtr == 0xb0) ||    // SUB$(.
                        (*srcPtr == 0xb6) ||    // SCALE(
                        (*srcPtr == 0xc0) ||    // REM
                        (*srcPtr == 0xe3))      // SPC(
                        firstData = true;
                    else
                        firstData = false;
                }
                //RTFBoldOff();
                RTFSetColor(kDefaultColor);

                if (*srcPtr == 0xc0) {
                    // REM -- do rest of line in green
                    RTFSetColor(kCommentColor);
                    inRem = true;
                }
            } else {
                /* simple chracter */
                if (*srcPtr == ':') // Reset line if we have a colon
                    firstData = true;
                if (!firstData) {
                    if (!literalYet) {
                        BufPrintf(" ");
                        literalYet = true;
                    }
                }
                if (fUseRTF) {
                    if (*srcPtr == '"' && !inRem) {
                        if (!inQuote) {
                            RTFSetColor(kStringColor);
                            RTFPrintChar(*srcPtr);
                        } else {
                            RTFPrintChar(*srcPtr);
                            RTFSetColor(kDefaultColor);
                        }
                        inQuote = !inQuote;
                    } else if (*srcPtr == ':' && !inRem && !inQuote) {
                        RTFSetColor(kColonColor);
                        RTFPrintChar(*srcPtr);
                        RTFSetColor(kDefaultColor);
                    } else {
                        RTFPrintChar(*srcPtr);
                    }
                } else {
                    BufPrintf("%c", *srcPtr);
                }
            }

            srcPtr++;
            length--;
        }

        if (inQuote || inRem)
            RTFSetColor(kDefaultColor);
        inQuote = inRem = false;

        srcPtr++;
        length--;

        if (!length) {
            LOGI("  BA3 truncated in mid-line");
            break;
        }

        RTFNewPara();
    }

done:
    RTFEnd();

    SetResultBuffer(pOutput);
    retval = 0;

//bail:
    return retval;
}
