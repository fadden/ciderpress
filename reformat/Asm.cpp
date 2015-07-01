/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Convert assembly source code.
 *
 * S-C Assembler, LISA, and Merlin-8 are handled here.  Others, such as
 * Orca/M, are either plain text or close enough that the "converted text"
 * code handles it well enough.
 */
#include "StdAfx.h"
#include "Asm.h"


/*
 * ===========================================================================
 *      S-C Assembler
 * ===========================================================================
 */

/*
 * S-C Assembler file format (thanks to Paul Schlyter, pausch at saaf.se):
 *
 *  <16-bit file length>  [DOS 3.3 only]
 *  <line> ...
 *
 * Each line consists of:
 *  <8-bit line length>
 *  <16-bit line number>
 *  <characters> ...
 *  <end-of-line token ($00)>
 *
 * Characters may be:
 *  $00-$1f: invalid
 *  $20-$7f: literal character
 *  $80-$bf: compressed spaces (0 to 63 count)
 *  $c0    : RLE token ($c0 <n> <ch> == repeat <ch> for <n> times)
 *  $c1-$ff: invalid
 *
 * There is no end-of-file marker.
 */

/*
 * Decide whether or not we want to handle this file.
 */
void ReformatSCAssem::Examine(ReformatHolder* pHolder)
{
    if (pHolder->GetFileType() == kTypeINT && pHolder->GetAuxType() == 0) {
        if (ReformatSCAssem::IsSCAssem(pHolder)) {
            /* definitely S-C assembler */
            pHolder->SetApplic(ReformatHolder::kReformatSCAssem,
                ReformatHolder::kApplicYes,
                ReformatHolder::kApplicNot, ReformatHolder::kApplicNot);
        } else {
            /* possibly S-C assembler */
            pHolder->SetApplic(ReformatHolder::kReformatSCAssem,
                ReformatHolder::kApplicMaybe,
                ReformatHolder::kApplicNot, ReformatHolder::kApplicNot);
        }
    } else {
        /* not S-C assembler */
        pHolder->SetApplic(ReformatHolder::kReformatSCAssem,
            ReformatHolder::kApplicNot,
            ReformatHolder::kApplicNot, ReformatHolder::kApplicNot);
    }
}

/*
 * Figure out if a type 'I' file is an Integer BASIC program or an S-C
 * assembler listing.
 *
 * They both have a line length and line number, but use different conventions
 * for marking the end of a line, and have different sets of valid chars.  We
 * don't need to fully validate the file, just test the first line.
 */
/*static*/ bool ReformatSCAssem::IsSCAssem(const ReformatHolder* pHolder)
{
    const uint8_t* ptr = pHolder->GetSourceBuf(ReformatHolder::kPartData);
    long srcLen = pHolder->GetSourceLen(ReformatHolder::kPartData);
    int len;

    len = *ptr;
    if (len == 0 || len > srcLen)
        return false;       // should return an error, really
    if (ptr[len-1] == 0x00) {
        LOGI("  Found 0x00, looks like S-C assembler");
        return true;
    } else if (ptr[len-1] == 0x01) {
        LOGI("  Found 0x01, looks like Integer BASIC");
        return false;
    } else {
        LOGI("  Got strange value 0x%02x during S-C test", ptr[len-1]);
        return false;       // again, should return an error
    }
}


/*
 * Reformat an S-C Assembler listing into text.  I don't know exactly what the
 * original listings looked like, so I'm just doing what A2FID.C does.
 */
int ReformatSCAssem::Process(const ReformatHolder* pHolder,
    ReformatHolder::ReformatID id, ReformatHolder::ReformatPart part,
    ReformatOutput* pOutput)
{
    const uint8_t* srcPtr = pHolder->GetSourceBuf(part);
    long srcLen = pHolder->GetSourceLen(part);
    long length = srcLen;
    // (this was written before tab stuff in ReformatAsm class existed)
    static const char* kSpaces64 =  "                                "
                                    "                                ";
    int retval = -1;

    fUseRTF = false;

    RTFBegin();

    /*
     * Make sure there's enough here to get started.  We want to return an
     * "okay" result because we want this treated like a reformatted empty
     * BASIC program rather than a non-Integer file.
     */
    if (length < 2) {
        LOGI("  SCAssem truncated?");
        BufPrintf("\r\n");
        goto done;
    }

    while (length > 0) {
        uint8_t lineLen;
        uint16_t lineNum;

        /* pull the length byte, which we sanity-check */
        lineLen = *srcPtr++;
        length--;
        if (lineLen == 0) {
            LOGI("  SCAssem found zero-length line?");
            break;
        }

        /* line number */
        lineNum = Read16(&srcPtr, &length);
        BufPrintf("%04u ", lineNum);

        while (*srcPtr != 0x00 && length > 0) {
            if (*srcPtr >= 0x20 && *srcPtr <= 0x7f) {
                BufPrintf("%c", *srcPtr);
            } else if (*srcPtr >= 0x80 && *srcPtr <= 0xbf) {
                BufPrintf("%s", kSpaces64 + (64+128 - *srcPtr));
            } else if (*srcPtr == 0xc0) {
                if (length > 2) {
                    int count = *(srcPtr+1);
                    uint8_t ch = *(srcPtr + 2);

                    srcPtr += 2;
                    length -= 2;
                    while (count--)
                        BufPrintf("%c", ch);
                } else {
                    LOGI("  SCAssem GLITCH: RLE but only %d chars left",
                        length);
                    BufPrintf("?!?");
                }
            } else {
                LOGI("  SCAssem invalid char 0x%02x", *srcPtr);
                BufPrintf("?");
            }

            srcPtr++;
            length--;
        }

        /* skip past EOL token */
        ASSERT(*srcPtr == 0x00 || length <= 0);
        srcPtr++;
        length--;

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
 *      Merlin 8 and Merlin 8/16 Assembler
 * ===========================================================================
 */

/*
 * Merlin source code uses ordinary text files that usually have names
 * ending in ".S".  They use high ASCII text -- unusual for ProDOS text
 * files -- with the occasional low-ASCII space character.
 *
 * We don't absolutely need this conversion, because the files are already
 * plain text, but it's easier to read when the various pieces are tabbed
 * to reasonable screen offsets.
 *
 * The 0xa0 values seem to be used to separate pieces, while the 0x20
 * values are used for comments and other filler.  It is entirely possible
 * to have a Merlin source file with no 0x20 values.
 */

/*
 * Decide whether or not we want to handle this file.  We know it's type
 * TXT, though the aux type can be almost anything.
 *
 * If we really just want Merlin we should probably exclude DOS disks,
 * since the text file contents will match.  However, it's probably useful
 * to support DOS ED/ASM sources with this.
 */
void ReformatMerlin::Examine(ReformatHolder* pHolder)
{
    if (pHolder->GetFileType() == kTypeTXT) {
        bool isAsm = ReformatMerlin::IsMerlin(pHolder);
        bool isDotS = stricmp(pHolder->GetNameExt(), ".S") == 0;

        if (isAsm && isDotS) {
            /* gotta be */
            pHolder->SetApplic(ReformatHolder::kReformatMerlin,
                ReformatHolder::kApplicYes,
                ReformatHolder::kApplicNot, ReformatHolder::kApplicNot);
        } else if (isAsm) {
            /* probably Merlin assembler, or at least *some* sort of asm */
            pHolder->SetApplic(ReformatHolder::kReformatMerlin,
                ReformatHolder::kApplicProbably,
                ReformatHolder::kApplicNot, ReformatHolder::kApplicNot);
        } else if (isDotS) {
            /* not likely, but offer it as non-default option */
            pHolder->SetApplic(ReformatHolder::kReformatMerlin,
                ReformatHolder::kApplicProbablyNot,
                ReformatHolder::kApplicNot, ReformatHolder::kApplicNot);
        } else {
            /* probably not Merlin, don't allow */
            pHolder->SetApplic(ReformatHolder::kReformatMerlin,
                ReformatHolder::kApplicNot,
                ReformatHolder::kApplicNot, ReformatHolder::kApplicNot);
        }
    } else {
        /* not S-C assembler */
        pHolder->SetApplic(ReformatHolder::kReformatMerlin,
            ReformatHolder::kApplicNot,
            ReformatHolder::kApplicNot, ReformatHolder::kApplicNot);
    }
}

/*
 * Figure out if the contents of this file match up with our expections
 * for Merlin source code.
 *
 * Specifically, does it use high ASCII and 0x20 exclusively, and does
 * it have a large number of lines that begin with a single space or the
 * comment token ('*')?
 *
 * Typical source files start with a space on 40-60% of lines, but "equates"
 * files and files that are substantially comments break the rule.
 *
 * This will also return "true" for DOS ED/ASM files.
 */
/*static*/ bool ReformatMerlin::IsMerlin(const ReformatHolder* pHolder)
{
    const uint8_t* ptr = pHolder->GetSourceBuf(ReformatHolder::kPartData);
    long srcLen = pHolder->GetSourceLen(ReformatHolder::kPartData);

    bool isLineStart = true;
    int lineCount, spaceLineCount, commentLineCount;

    lineCount = spaceLineCount = commentLineCount = 0;
    while (srcLen--) {
        if ((*ptr & 0x80) == 0 && (*ptr != 0x20)) {
            LOGI("  Merlin: not, found 0x%02x", *ptr);
            return false;
        }

        if (isLineStart) {
            lineCount++;

            if ((*ptr & 0x7f) == 0x20 && srcLen != 0 &&
                (*(ptr+1) & 0x7f) != 0x20)
                spaceLineCount++;
            if (*ptr == 0xaa)       // '*'
                commentLineCount++;
            isLineStart = false;
        }

        if (*ptr == 0x8d)
            isLineStart = true;

        ptr++;
    }

    if (!lineCount)
        return false;       // don't divide by zero

    LOGI("  Merlin: found %d lines", lineCount);
    LOGI("    %d start with spaces (%.3f%%), %d with comments (%.3f%%)",
        spaceLineCount, (spaceLineCount * 100.0) / lineCount,
        commentLineCount, (commentLineCount * 100.0) / lineCount);

    if ((spaceLineCount * 100) / lineCount > 40)
        return true;
    if (((spaceLineCount + commentLineCount) * 100) / lineCount > 50)
        return true;
    return false;
}


/*
 * Re-tab a Merlin assembly file.
 *
 * We try to track quoted material on the operand field to avoid tabbing
 * parts of quoted text around.  This isn't strictly necessary for a well-
 * formed Merlin file, which uses 0x20 as a "non-breaking space", but if it
 * has been "washed" through a converter or if this is actually a DOS ED/ASM
 * file, tracking quotes is almost always beneficial.
 */
int ReformatMerlin::Process(const ReformatHolder* pHolder,
    ReformatHolder::ReformatID id, ReformatHolder::ReformatPart part,
    ReformatOutput* pOutput)
{
    const uint8_t* srcPtr = pHolder->GetSourceBuf(part);
    long srcLen = pHolder->GetSourceLen(part);
    int retval = -1;
    enum { kStateLabel, kStateMnemonic, kStateOperand, kStateComment };
    int tabStop[] = { 0, 9, 15, 26 };   // 1:1 map with state enum
    int state;
    uint8_t quoteChar = '\0';

    fUseRTF = false;

    RTFBegin();

    bool isLineStart = true;
    for ( ; srcLen > 0; srcLen--, srcPtr++) {
        bool wasLineStart = false;
        if (isLineStart) {
            isLineStart = false;
            wasLineStart = true;
            OutputStart();      // begin new line in output buffer
            state = kStateLabel;
            if (*srcPtr == 0xaa) {
                // leading '*' makes entire line a comment
                state = kStateComment;
            }
        }
        if (*srcPtr == 0x8d) {
            OutputFinish();     // end of line

            BufPrintf("%s", GetOutBuf());
            RTFNewPara();

            isLineStart = true;
            if (quoteChar != '\0') {
                // unterminated quote
                DebugBreak();
                quoteChar = '\0';
            }
            continue;
        }

        if (state >= kStateComment) {
            Output(*srcPtr & 0x7f);
        } else if (quoteChar != '\0') {
            if (*srcPtr == quoteChar) {
                /* close quote */
                quoteChar = '\0';
            }
            Output(*srcPtr & 0x7f);
        } else if (state == kStateOperand &&
                   (*srcPtr == '\'' + 0x80 || *srcPtr == '"' + 0x80))
        {
            /* open quote */
            quoteChar = *srcPtr;
            Output(quoteChar & 0x7f);
        } else if (*srcPtr == 0xa0) {       // high-ASCII space
            // does not trigger on 0x20; this matches behavior of
            // Merlin-16 v3.40
            state++;
            OutputTab(tabStop[state]);
        } else if (*srcPtr == 0xbb &&
                (wasLineStart || *(srcPtr-1) == 0xa0)) {
            // Found a high-ASCII ';' at the start of the line or right after
            // a space.  Semicolons can appear in the middle of macros, so
            // we need the extra test to avoid introducing a column break.
            //
            // just comment, or comment on mnemonic w/o operand
            // (shouldn't tab out if line started with label but
            // contains 0x20s instead of 0xa0s between components;
            // oh well.)
            state = kStateComment;
            OutputTab(tabStop[state]);
            Output(*srcPtr & 0x7f);
        } else {
            Output(*srcPtr & 0x7f);
        }
    }

    RTFEnd();

    SetResultBuffer(pOutput);
    retval = 0;
    return retval;
}


/*
 * ===========================================================================
 *      LISA Assembler - v2.x
 * ===========================================================================
 */

/*
 * This is for LISA v2.5 and earlier, which ran under DOS 3.3.  It used a
 * fairly simple format with tokenized mnemonics.
 *
 * The conversion was created by examination of the source files.  The table
 * of mnemonics was extracted from the assembler binary. 
 */

/*
 * Decide whether or not we want to handle this file.
 */
void ReformatLISA2::Examine(ReformatHolder* pHolder)
{
    if (pHolder->GetSourceFormat() == ReformatHolder::kSourceFormatDOS &&
        pHolder->GetFileType() == kTypeDOS_B)
    {
        if (ReformatLISA2::IsLISA(pHolder)) {
            /* definitely LISA */
            pHolder->SetApplic(ReformatHolder::kReformatLISA2,
                ReformatHolder::kApplicYes,
                ReformatHolder::kApplicNot, ReformatHolder::kApplicNot);
        } else {
            /* maybe LISA */
            pHolder->SetApplic(ReformatHolder::kReformatLISA2,
                ReformatHolder::kApplicMaybe,
                ReformatHolder::kApplicNot, ReformatHolder::kApplicNot);
        }
    } else {
        /* not LISA */
        pHolder->SetApplic(ReformatHolder::kReformatLISA2,
            ReformatHolder::kApplicNot,
            ReformatHolder::kApplicNot, ReformatHolder::kApplicNot);
    }
}

/*
 * Quick sanity check on the file contents.
 */
bool ReformatLISA2::IsLISA(const ReformatHolder* pHolder)
{
    const uint8_t* srcPtr = pHolder->GetSourceBuf(ReformatHolder::kPartData);
    long srcLen = pHolder->GetSourceLen(ReformatHolder::kPartData);
    uint16_t version, len;

    if (srcLen < 8)
        return false;

    version = Read16(&srcPtr, &srcLen);
    len = Read16(&srcPtr, &srcLen);

    if (len > srcLen)
        return false;

    return true;
}


/*
 * Opcode mnemonics.
 */
static const char gOpcodes[] = 
    "BGEBLTBMIBCCBCSBPLBNEBEQ"      // 80-87
    "BVSBVCBSBBNMBM1BNZBIZBIM"      // 88-8f
    "BIPBICBNCBRABTRBFLBRKBKS"      // 90-97
    "CLVCLCCLDCLIDEXDEYINXINY"      // 98-9f
    "NOPPHAPLAPHPPLPRTSRTIRSB"      // a0-a7
    "RTNSECSEISEDTAXTAYTSXTXA"      // a8-af
    "TXSTYAADDCPRDCRINRSUBLDD"      // b0-b7
    "POPPPDSTDSTPLDRSTOSET___"      // b8-bf
    "ADCANDORABITCMPCPXCPYDEC"      // c0-c7
    "EORINCJMPJSR___LDALDXLDY"      // c8-cf
    "STASTXSTYXORLSRRORROLASL"      // d0-d7
    "ADREQUORGOBJEPZSTRDCMASC"      // d8-df
    "ICLENDLSTNLSHEXBYTHBYPAU"      // e0-e7
    "DFSDCI...PAGINVBLKDBYTTL"      // e8-ef
    "SBC___LET.IF.EL.FI=  PHS"      // f0-f7
    "DPH.DAGENNOGUSR_________"      // f8-ff
    ;


/*
 * Format:
 *  2-byte version (?)
 *  2-byte length
 *  <LINE> ...
 *
 * Each line is:
 *  1-byte length
 *  <DATA>
 *  CR
 *
 * Last line has length=255.
 */

/*
 * Parse a file.
 */
int ReformatLISA2::Process(const ReformatHolder* pHolder,
    ReformatHolder::ReformatID id, ReformatHolder::ReformatPart part,
    ReformatOutput* pOutput)
{
    const uint8_t* srcPtr = pHolder->GetSourceBuf(part);
    long srcLen = pHolder->GetSourceLen(part);
    long actualLen;
    int retval = -1;

    fUseRTF = false;

    if (srcLen < 8) {
        LOGI("  LISA truncated?");
        goto bail;
    }

    uint16_t version;

    version = Read16(&srcPtr, &srcLen);     // usually 0x1800; maybe "2.4"?
    actualLen = Read16(&srcPtr, &srcLen);

    LOGI("  LISA version 0x%04x, len=%d", version, actualLen);

    if (actualLen > srcLen) {
        LOGI("  LISA bad length (len=%ld actual=%ld)", srcLen, actualLen);
        goto bail;
    }

    int lineNum;
    lineNum = 0;
    while (actualLen > 0) {
        int lineLen = *srcPtr;
        if (lineLen == 0) {
            LOGI("  LISA bad line len (%ld)", lineLen);
            break;
        } else if (lineLen == 255) {
            // used as end-of-file marker
            break;
        }

        lineNum++;

        OutputStart();
        ProcessLine(srcPtr);
        OutputFinish();

        //BufPrintf("%4d %s\r\n", lineNum, GetOutBuf());
        BufPrintf("%s\r\n", GetOutBuf());

        srcPtr += lineLen+1;
        actualLen -= lineLen+1;
    }

    SetResultBuffer(pOutput);
    retval = 0;

bail:
    return retval;
}

void ReformatLISA2::ProcessLine(const uint8_t* buf)
{
    int len = *buf;
    uint8_t uch;

    // consume length byte
    buf++;
    len--;

    if (*buf >= 0x80) {
        // starting the opcode, tab past label field
        OutputTab(kOpTab);
    } else if (*buf != ';' && len > 8) {
        // starting with 8-character label
        bool doPrint = true;
        for (int i = 0; i < 8; i++) {
            uch = *buf;
            if (uch < 0x20 || uch >= 0x80) {
                LOGI("  LISA funky char 0x%02x in label", uch);
                break;
            } else if (uch == 0x20) {
                doPrint = false;
            }
            if (doPrint)
                Output(uch);
            buf++;
            len--;
        }
        if (len > 0 && *buf == ':') {
            Output(*buf);
            buf++;
            len--;
        }
        OutputTab(kOpTab);
    }

    bool mnemonicDone = false;
    bool operandDone = false;
    while (len--) {
        uch = *buf++;

        if (uch >= 0x20 && uch < 0x80) {
            if (mnemonicDone && uch != 0x20)
                operandDone = true;
            if (mnemonicDone && !operandDone && uch == 0x20) {
                // suppress extra spaces between mnemonic and operand
            } else
                Output(uch);
        } else if (uch < 0x20) {
            // Values from 0x01 - 0x05 are used to separate the opcode from
            // the operand, and seem to "hint" the operand type (immediate,
            // absolute, etc).  Just ignore for now.
        } else if (uch == 0x0d) {
            // don't output CR to line buf
            if (len) {
                LOGI("WARNING: got early CR");
            }
        } else if (mnemonicDone) {
            // Values >= 0x80 are mnemonics, but we've already seen it.
            // LISA seems to use 0xbb to separate operand and comment field
            // (would be "STP" mnemonic).  I don't see other uses, so I'm
            // just going to tab over instead of outputing a second
            // mnemonic value.
            if (len > 1) {
                OutputTab(kComTab);
                Output(';');
            }
        } else {
            const char* mnemonic;

            mnemonic = &gOpcodes[(uch - 128) * 3];
            Output(mnemonic[0]);
            Output(mnemonic[1]);
            Output(mnemonic[2]);
            OutputTab(kAdTab);
            mnemonicDone = true;
        }
    }
}


/*
 * ===========================================================================
 *      LISA Assembler - v3
 * ===========================================================================
 */

/*
 * The ProDOS version of LISA uses the INT filetype with the assembler
 * version number in the aux type.  The version is always < $4000.
 *
 * The file format looks like this:
 *  4-byte header
 *  symbol dictionary, 8 bytes per symbol
 *  <line> ...
 *
 * The way the lines are decoded is fairly involved.  The code here was
 * developed from the LISA v3.2a sources, as found on the A2ROMulan CD-ROM.
 */

/*
 * Opcode mnemonics.
 */
static const char gMnemonics3[256*3 +1] =
    // 0x00 (SN, M65.2) - Group 1 instructions
    "addadcandcmpeorldaorasbc"
    "stasubxor"
    // 0x0b - Group 2 instructions
             "asldecinclsrrol"
    "ror"
    // 0x11 - Group 3 instructions
       ".ifwhlbrabccbcsbeqbfl"
    "bgebltbmibnebplbtrbvcbvs"
    "jsrobjorgphs"
    // 0x24 - Group 4 instructions
                ".mdfzrinplcl"
    "rls"
    // 0x29 - Group 5 instructions
       "bitcpxcpyjmpldxldystx"
    "stytrbtsbstz"
    // 0x34 - Group 6 instructions
                "=  conepzequ"
    "set"
    // 0x39 - Group 7 instructions
       ".daadrbytcspdbyhby"
    // 0x3f - Group 8 instructions
                         "anx"
    "sbtttlchnblkdciinvrvsmsg"
    "strzro"
    // 0x4a - Group 9 instructions
          "dfshexusrsav"
    //M65LEN2  equ      * - M65.2
                      "??????"      // 0x4e-0x4f
    "????????????????????????"      // 0x50-0x57
    "????????????????????????"      // 0x58-0x5f
    "????????????????????????"      // 0x60-0x67
    "????????????????????????"      // 0x68-0x6f
    "????????????????????????"      // 0x70-0x77
    "????????????????????????"      // 0x78-0x7f
    "????????????????????????"      // 0x80-0x87
    "????????????????????????"      // 0x88-0x8f
    "????????????????????????"      // 0x90-0x97
    "????????????????????????"      // 0x98-0x9f
    "????????????????????????"      // 0xa0-0xa7
    "????????????????????????"      // 0xa8-0xaf

    // 0xb0 (SS M65.1) - assembler directives
    ".el.fi.me.wedphif1if2end"
    "expgenlstnlsnognoxpagpau"
    "nlccnd   "
    // 0xc2 - Single-byte instructions
             "asllsrrolrordec"
    "incbrkclccldcliclvdexdey"
    "inxinynopphaphpplaplprti"
    "rtssecsedseitaxtaytsxtxa"
    "txstyaphxphyplxply"
    //M65LEN1  equ      * - M65.1

                      "??????"      // 0xe6-0xe7
    "????????????????????????"      // 0xe8-0xef
    "????????????????????????"      // 0xf0-0xff
    "????????????????????????"      // 0xf8-0xff
    ;

/*
 * Determine whether this is one of our files.
 */
void ReformatLISA3::Examine(ReformatHolder* pHolder)
{
    /*
     * Note we cannot false-positive on an INT file on a DOS disk, because
     * in DOS 3.3 INT files always have zero aux type.
     */
    if (pHolder->GetFileType() == kTypeINT &&
        pHolder->GetAuxType() < 0x4000)
    {
        if (ReformatLISA3::IsLISA(pHolder)) {
            /* definitely LISA */
            pHolder->SetApplic(ReformatHolder::kReformatLISA3,
                ReformatHolder::kApplicYes,
                ReformatHolder::kApplicNot, ReformatHolder::kApplicNot);
        } else {
            /* possibly LISA */
            pHolder->SetApplic(ReformatHolder::kReformatLISA3,
                ReformatHolder::kApplicMaybe,
                ReformatHolder::kApplicNot, ReformatHolder::kApplicNot);
        }
    } else {
        /* not LISA */
        pHolder->SetApplic(ReformatHolder::kReformatLISA3,
            ReformatHolder::kApplicNot,
            ReformatHolder::kApplicNot, ReformatHolder::kApplicNot);
    }
}

/*
 * Decide if this is one of ours or perhaps an Integer BASIC or S-C
 * assembler source.
 */
/*static*/ bool ReformatLISA3::IsLISA(const ReformatHolder* pHolder)
{
    bool dosStructure = (pHolder->GetSourceFormat() == ReformatHolder::kSourceFormatDOS);
    const uint8_t* srcPtr = pHolder->GetSourceBuf(ReformatHolder::kPartData);
    long srcLen = pHolder->GetSourceLen(ReformatHolder::kPartData);

    if (pHolder->GetSourceFormat() == ReformatHolder::kSourceFormatDOS)
        return false;       // can only live under ProDOS; need len + aux type

    if (srcLen < kHeaderLen+2)
        return false;       // too short

    uint16_t codeLen, symLen;

    codeLen = srcPtr[0x00] | srcPtr[0x01] << 8;
    symLen = srcPtr[0x02] | srcPtr[0x03] << 8;

    if ((symLen & 0x0003) != 0 || symLen > 512*8 || symLen > srcLen) {
        LOGI("  LISA3 bad symLen");
        return false;
    }
    if (codeLen > srcLen) {
        LOGI("  LISA3 funky codeLen");
        return false;
    }
    if (codeLen + symLen + kHeaderLen > srcLen) {
        LOGI("  LISA3 bad combined len");
        return false;
    }

    return true;
}

/*
 * Parse a file.
 */
int ReformatLISA3::Process(const ReformatHolder* pHolder,
    ReformatHolder::ReformatID id, ReformatHolder::ReformatPart part,
    ReformatOutput* pOutput)
{
    const uint8_t* srcPtr = pHolder->GetSourceBuf(part);
    long srcLen = pHolder->GetSourceLen(part);
    int retval = -1;

    if (srcLen < kHeaderLen+2) {
        LOGI("  LISA3 too short");
        goto bail;
    }

    fUseRTF = false;

    uint16_t codeLen, symLen;

    codeLen = srcPtr[0x00] | srcPtr[0x01] << 8;
    symLen = srcPtr[0x02] | srcPtr[0x03] << 8;

    printf("codeLen=%d, symLen=%d\n", codeLen, symLen);

    if ((symLen & 0x0003) != 0 || symLen > 512*8 || symLen > srcLen) {
        LOGI("  LISA3 bad symLen");
        goto bail;
    }
    if (codeLen > srcLen) {
        LOGI("  LISA3 funky codeLen");
        goto bail;
    }
    if (codeLen + symLen + kHeaderLen > srcLen) {
        LOGI("  LISA3 bad combined len");
        goto bail;
    }

    fSymCount = symLen / 8;
    fSymTab = srcPtr + kHeaderLen;
#if 0
    int ii;
    for (ii = 0; ii < fSymCount; ii++) {
        OutputStart();
        PrintSymEntry(ii);
        OutputFinish();
        LOGI("%d: %hs", ii, GetOutBuf());
    }
#endif

    /*
     * Do stuff with source lines.
     */
    const uint8_t* codePtr;
    const uint8_t* endPtr;
    int lineNum;

    codePtr = srcPtr + kHeaderLen + symLen;
    endPtr = srcPtr + srcLen;
    assert(codePtr < endPtr);
    lineNum = 0;

    while (codePtr < endPtr) {
        uint8_t flagByte;
        int lineLen;

        OutputStart();
        lineNum++;

#if 0
        {
            char offbuf[12];
            sprintf(offbuf, "0x%04x", codePtr - srcPtr);
            Output(offbuf);
            Output('-');
        }
#endif

        flagByte = *codePtr++;
        if (flagByte < 0x80) {
            /* BIGONE - explicit length, complex line */
            lineLen = flagByte;
            /* subtract 1 from flagByte, because len includes flagByte */
            if (flagByte > 0)
                ProcessLine(codePtr, flagByte-1);
        } else {
            /* SPCLCASE - locals, labels, comments */
            if (flagByte >= kLCLTKN) {
                lineLen = 1;
                if (flagByte == kCMNTTKN+1) {
                    Output(';');
                } else if (flagByte == kCMNTTKN) {
                    Output('*');
                } else if (flagByte < kLBLTKN) {
                    /* CNVRTLCL: 0xf0 - 0xf9 - local numeric labels */
                    Output('^');
                    Output('0' + flagByte - 0xf0);
                    Output(':');
                } else if (flagByte < kMACTKN) {
                    /* normal label; 0xfb means add 256 */
                    int idx;
                    idx = *codePtr | (flagByte & 0x01) << 8;
                    PrintSymEntry(idx);
                    Output(':');
                    lineLen = 2;
                } else {
                    /* macro (only object on line) */
                    assert(flagByte == kMACTKN || flagByte == kMACTKN+1);
                    OutputTab(kOpTab);
                    int idx;
                    idx = *codePtr | (flagByte & 0x01) << 8;
                    Output('/');        // MACROCHR
                    PrintSymEntry(idx);
                    lineLen = 2;
                }
            } else {
                /* SHRTMNM2 - simple, standard mnemonic */
                lineLen = 1;
                OutputTab(kOpTab);
                PrintMnemonic(flagByte);
            }
        }
        

        if (lineLen == 0) {
            /* end of file */
            break;
        }

        OutputFinish();
        //BufPrintf("%d: %s\r\n", lineNum, outBuf);
        BufPrintf("%s\r\n", GetOutBuf());

        codePtr += lineLen-1;
    }

    LOGI("codePtr=0x%p endPtr=%p numLines=%d", codePtr, endPtr, lineNum-1);
    LOGI("extra = %d", endPtr - codePtr);

    SetResultBuffer(pOutput);
    retval = 0;

bail:
    fSymTab = NULL;
    return retval;
}


/*
 * BIGONE
 */
void ReformatLISA3::ProcessLine(const uint8_t* codePtr, int len)
{
    uint8_t mnemonic = 0;

    //printf("{code=0x%02x len=%d}", *codePtr, len);
    if (*codePtr == kCMNTTKN+1 || *codePtr == kCMNTTKN) {
        switch (*codePtr) {
        case kCMNTTKN+1:    Output(';');    break;
        case kCMNTTKN:      Output('*');    break;
        default:
            assert(false);
        }
        // CNVCMNT
        codePtr++;
        while (--len)
            Output(*codePtr++ & 0x7f);

        goto bail;
    } else if (*codePtr == kMACTKN || *codePtr == kMACTKN+1) {
        /* CHKMACRO - handle macro */
        uint16_t idx;
        mnemonic = *codePtr;
        idx = (*codePtr & 0x01) << 8;
        idx |= *++codePtr;
        OutputTab(kOpTab);
        Output('/');        // MACROCHR
        PrintSymEntry(idx);
        codePtr++;
        len -= 2;
        goto ConvtOperand;
    } else if (*codePtr == kLBLTKN || *codePtr == kLBLTKN+1) {
        /* CHKCLBL - handle label at start of line */
        uint16_t idx;
        idx = (*codePtr & 0x01) << 8;
        idx |= *++codePtr;
        PrintSymEntry(idx);
        codePtr++;
        len -= 2;
        // goto ConvtMnem
    } else if (*codePtr >= kLCLTKN) {
        /* CHKLLBL - handle local label (^) */
        Output('^');
        Output((char) (*codePtr - 0xc0));
        codePtr++;
        len--;
        // goto CNVTMNEM
    } else {
        /* no label; current value is the mnemonic; continue w/o advancing */
        // fall through to CNVTMNEM
    }

    /* CNVTMNEM */
    mnemonic = *codePtr++;
    len--;
    //printf("{mne=0x%02x}", mnemonic);
    if (mnemonic >= kMACTKN) {
        /* CNVRTMAC */
        assert(mnemonic == kMACTKN || mnemonic == kMACTKN+1);
        OutputTab(kOpTab);
        int idx;
        idx = *codePtr++;
        idx |= (mnemonic & 0x01) << 8;
        Output('/');        // MACROCHR
        PrintSymEntry(idx);
        len--;
        //printf("{MAC:%d}", len);
    } else {
        OutputTab(kOpTab);
        PrintMnemonic(mnemonic);
    }

ConvtOperand:
    /* ConvtOperand */
    //printf("{cen=%d}", len);
    ConvertOperand(mnemonic, &codePtr, &len);

bail:
    //if (len > 0)
    //    LOGI("{LEN=%d}", len);

    return;
}


/*
 * CNVOPRND
 */
void ReformatLISA3::ConvertOperand(uint8_t mnemonic,
    const uint8_t** pCodePtr, int* pLen)
{
    static const char kOPRTRST1[] = "+-*/&|^=<>%<><";
    static const char kOPRTRST2[] = "\0\0\0\0\0\0\0\0\0\0\0==>";

    const uint8_t* codePtr = *pCodePtr;
    int len = *pLen;
    OperandResult result;
    uint8_t adrsMode = 0;
    uint8_t val;

    //printf("{opr len=%d}", len);

    if (mnemonic >= kCMNTTKN) {
        /* OUTCMNT2 */
        PrintComment(adrsMode, codePtr, len);
        goto bail;
    }
    if (mnemonic < kSS) {
        if (mnemonic < kGROUP3 || (mnemonic >= kGROUP5 && mnemonic < kGROUP6)) {
            // address mode is explicit
            adrsMode = *codePtr++;
            len--;
            //printf("{adrs=0x%02x}", adrsMode);
        }
    }
    OutputTab(kAdTab);
    if (adrsMode >= 0x10 && adrsMode < 0x80)
        Output('(');

    /* OUTOPRND */
    while (len > 0) {
        val = *codePtr++;
        len--;

        if (val == 0x0e) {
            Output('~');
            continue;       // goto OutOprnd
        } else if (val == 0x0f) {
            Output('-');
            continue;       // goto OutOprnd
        } else if (val == 0x3a) {
            Output('#');
            continue;       // goto OutOprnd
        } else if (val == 0x3b) {
            Output('/');
            continue;       // goto OutOprnd
        } else if (val == 0x3d) {
            Output('@');
            continue;       // goto OutOprnd
        } else {
            result = PrintNum(adrsMode, val, &codePtr, &len);
            if (result == kResultGotoOutOprnd)
                continue;   //goto OutOprnd;
            else if (result == kResultFailed)
                goto bail;
            // else goto OutOprtr
        }

OutOprtr:
        uint8_t opr;

        if (!len)
            break;
        opr = *codePtr++;
        len--;

        if (opr < 0x0e) {
            Output(' ');
            Output(kOPRTRST1[opr]);
            if (kOPRTRST2[opr] != '\0')
                Output(kOPRTRST2[opr]);
            Output(' ');
            // goto OutOprnd
        } else if (opr < 0x20 || opr >= 0x30) {
            // NOOPRTR
            if (opr == kCMNTTKN+1) {
                PrintComment(adrsMode, codePtr, len);
                codePtr += len;
                len = 0;
                goto bail;
            }
            Output(',');
            codePtr--;      // back up
            len++;
            // goto OutOprnd
        } else {
            Output('+');
            result = PrintNum(adrsMode, opr - 0x10, &codePtr, &len);
            if (result == kResultGotoOutOprnd)
                continue;
            else if (result == kResultGotoOutOprtr)
                goto OutOprtr;
            else
                goto bail;
        }
    }
    PrintComment(adrsMode, codePtr, len);

bail:
    *pCodePtr = codePtr;
    *pLen = len;
}

/*
 * Output a single byte as a binary string.
 */
void ReformatLISA3::PrintBin(uint8_t val)
{
    char buf[9];
    buf[8] = '\0';

    for (int bit = 0; bit < 8; bit++)
        buf[bit] = '0' + ((val >> (7-bit)) & 0x01);
    Output(buf);
}

/*
 * OUTNUM
 */
ReformatLISA3::OperandResult ReformatLISA3::PrintNum(int adrsMode, uint8_t val,
    const uint8_t** pCodePtr, int* pLen)
{
    const uint8_t* codePtr = *pCodePtr;
    int len = *pLen;
    OperandResult result = kResultUnknown;
    char numBuf[12];

    // OUTNUM - these all jump to OutOprtr unless otherwise specified
    if (val < 0x1a) {
        Output(val | '0');
    } else if (val == 0x1a) {
        // 1-byte decimal
        sprintf(numBuf, "%u", *codePtr++);
        Output(numBuf);
        len--;
    } else if (val == 0x1b) {
        // 2-byte decimal
        uint16_t num;
        num = *codePtr++;
        num |= *codePtr++ << 8;
        len -= 2;
        sprintf(numBuf, "%u", num);
        Output(numBuf);
    } else if (val == 0x1c) {
        // 1-byte hex
        Output('$');
        sprintf(numBuf, "%02X", *codePtr++);
        Output(numBuf);
        len--;
    } else if (val == 0x1d) {
        // 2-byte hex
        Output('$');
        uint16_t num;
        num = *codePtr++;
        num |= *codePtr++ << 8;
        sprintf(numBuf, "%04X", num);
        Output(numBuf);
        len -= 2;
    } else if (val == 0x1e) {
        Output('%');
        PrintBin(*codePtr++);
        len--;
    } else if (val == 0x1f) {
        Output('%');
        PrintBin(*codePtr++);
        PrintBin(*codePtr++);
        len -= 2;
    } else if (val >= 0x36 && val <= 0x39) {
        // OUTIMD
        if (val == 0x36 || val == 0x37)
            Output('#');
        else
            Output('/');
        int idx;
        idx = (val & 0x01) << 8;
        idx |= *codePtr++;
        PrintSymEntry(idx);
        len--;
    } else if (val == 0x3c) {
        Output('*');        // loc cntr token
    } else if (val < 0x4a) {
        // <0..<9 tokens
        Output('<');
        Output(val - 0x10);
    } else if (val < 0x50) {
        // ?0..?5 tokens (+0x66)
        Output('?');
        Output(val - 0x1a);
    } else if (val < 0x5a) {
        Output('>');
        Output(val - 0x20);
    } else if (val < 0x60) {
        // ?6..?9 tokens (+0x5c)
        uint8_t newVal = val - 0x24;
        Output('?');
        if (newVal == ';')
            Output('#');
        else
            Output(newVal);
        if (newVal == ':')
            result = kResultGotoOutOprnd;
    } else if (val < 0x80) {
        // String tokens
        int strLen = val & 0x1f;
        if (strLen == 0) {
            // explict length
            strLen = *codePtr++;
            len--;
        }
        if (strLen > len) {
            Output("!BAD STR!");
            DebugBreak();
            result = kResultFailed;
            goto bail;
        }
        char delim;
        if (*codePtr >= 0x80)
            delim = '"';
        else
            delim = '\'';
        Output(delim);
        while (strLen--) {
            if ((*codePtr & 0x7f) == delim)
                Output(delim);
            Output(*codePtr++ & 0x7f);
            len--;
        }
        Output(delim);
    } else if (val == kLBLTKN || val == kLBLTKN+1) {
        int idx;
        idx = (val & 0x01) << 8;
        idx |= *codePtr++;
        len--;
        PrintSymEntry(idx);
    } else if (val == kCMNTTKN+1) {
        /* OUTCMNT2 */
        PrintComment(adrsMode, codePtr, len);
        codePtr += len;
        len = 0;
    } else {
        // just go to OutOprtr
    }

    if (result == kResultUnknown)
        result = kResultGotoOutOprtr;

bail:
    *pCodePtr = codePtr;
    *pLen = len;
    return result;
}

/*
 * Print symbol table entry.  Each entry is an 8-byte label packed into
 * 6 bytes.
 */
void ReformatLISA3::PrintSymEntry(int ent)
{
    if (ent < 0 || ent >= fSymCount) {
        Output("!BAD SYM!");
        LOGI("invalid entry %d (max %d)", ent, fSymCount);
        DebugBreak();
        return;
    }

    const uint8_t* packed = &fSymTab[ent * 8];
    uint8_t tmp[8];
    int i;

    tmp[0] = packed[0] >> 2;
    tmp[1] = ((packed[0] << 4) & 0x3c) | packed[1] >> 4;
    tmp[2] = ((packed[1] << 2) & 0x3c) | packed[2] >> 6;
    tmp[3] = packed[2] & 0x3f;

    tmp[4] = packed[3] >> 2;
    tmp[5] = ((packed[3] << 4) & 0x3c) | packed[4] >> 4;
    tmp[6] = ((packed[4] << 2) & 0x3c) | packed[5] >> 6;
    tmp[7] = packed[5] & 0x3f;

    for (i = 0; i < 8; i++) {
        if (tmp[i] == 0x20)
            break;
        else if (tmp[i] >= 0x20)
            Output(tmp[i]);
        else
            Output(tmp[i] | 0x40);
    }
}

void ReformatLISA3::PrintMnemonic(uint8_t val)
{
    const char* ptr = &gMnemonics3[val * 3];
    Output(ptr[0]);
    Output(ptr[1]);
    Output(ptr[2]);
}

/*
 * OUTCMNT2
 *
 * Prints the comment.  Finishes off the operand if necessary.
 */
void ReformatLISA3::PrintComment(int adrsMode, const uint8_t* codePtr, int len)
{
    assert(len >= 0);

    if (adrsMode == 0x04)
        Output(",X");
    else if (adrsMode == 0x08)
        Output(",Y");
    else if (adrsMode == 0x10)
        Output(')');
    else if (adrsMode == 0x20)
        Output(",X)");
    else if (adrsMode == 0x40)
        Output("),Y");

    if (len > 0) {
        OutputTab(kComTab);
        Output(';');
        while (len--)
            Output(*codePtr++ & 0x7f);
    }
}


/*
 * ===========================================================================
 *      LISA Assembler - v4 and v5
 * ===========================================================================
 */

/*
 * The ProDOS / GS/OS version of LISA uses the INT filetype with the
 * assembler version number in the aux type.  The version is always > $4000.
 *
 * The file format looks like this:
 *  16-byte header
 *  symbol dictionary
 *  <line> ...
 *
 * The way the lines are decoded is fairly involved.  The code here was
 * developed from the LISA/816 v5.0a (433) sources, as found on
 * the A2ROMulan CD-ROM.
 */

/*
 * Determine whether this is one of our files.
 */
void ReformatLISA4::Examine(ReformatHolder* pHolder)
{
    /*
     * Note we cannot false-positive on an INT file on a DOS disk, because
     * in DOS 3.3 INT files always have zero aux type.
     */
    if (pHolder->GetFileType() == kTypeINT &&
        pHolder->GetAuxType() >= 0x4000)
    {
        if (ReformatLISA4::IsLISA(pHolder)) {
            /* definitely LISA */
            pHolder->SetApplic(ReformatHolder::kReformatLISA4,
                ReformatHolder::kApplicYes,
                ReformatHolder::kApplicNot, ReformatHolder::kApplicNot);
        } else {
            /* possibly LISA */
            pHolder->SetApplic(ReformatHolder::kReformatLISA4,
                ReformatHolder::kApplicMaybe,
                ReformatHolder::kApplicNot, ReformatHolder::kApplicNot);
        }
    } else {
        /* not LISA */
        pHolder->SetApplic(ReformatHolder::kReformatLISA4,
            ReformatHolder::kApplicNot,
            ReformatHolder::kApplicNot, ReformatHolder::kApplicNot);
    }
}

/*
 * Decide if this is one of ours or perhaps an Integer BASIC or S-C
 * assembler source.
 */
/*static*/ bool ReformatLISA4::IsLISA(const ReformatHolder* pHolder)
{
    bool dosStructure = (pHolder->GetSourceFormat() == ReformatHolder::kSourceFormatDOS);
    const uint8_t* srcPtr = pHolder->GetSourceBuf(ReformatHolder::kPartData);
    long srcLen = pHolder->GetSourceLen(ReformatHolder::kPartData);

    if (pHolder->GetSourceFormat() == ReformatHolder::kSourceFormatDOS)
        return false;       // can only live under ProDOS; need len + aux type

    if (srcLen < kHeaderLen+2)
        return false;       // too short

    uint16_t version;
    uint16_t symEnd;
    uint16_t symCount;

    version = srcPtr[0x00] | srcPtr[0x01] << 8;
    symEnd = srcPtr[0x02] | srcPtr[0x03] << 8;
    symCount = srcPtr[0x04] | srcPtr[0x05] << 8;

    if (symEnd > srcLen) {
        LOGI("  LISA4 bad symEnd");
        return false;
    }
    if (symCount > symEnd) {
        LOGI("  LISA4 funky symCount (count=%d end=%d)",
            symCount, symEnd);
        return false;;
    }

    uint8_t opTab, adTab, comTab;
    opTab = srcPtr[0x06];
    adTab = srcPtr[0x07];
    comTab = srcPtr[0x08];

    if (opTab < 1 || adTab < 2 || comTab < 3) {
        LOGI("  LISA4 missing tabs");
        return false;
    }
    if (opTab >= 128 || adTab >= 128 || comTab >= 128) {
        LOGI("  LISA4 huge tabs");
        return false;
    }

    return true;
}

static const char gHexDigit[] = "0123456789ABCDEF";


/*
 * Table of mnemonics, from v5.0a editor sources.
 *
 * Some entries were not present in the editor sources, but were used
 * by sample source code, and have been added here:
 *  0x6c .assume
 *  0x7f .table
 */
static const char* gMnemonics4[] = {
    // 00 - 0f
    "???", "add", "adc", "and", "cmp", "eor", "lda", "ora",
    "sbc", "sta", "sub", "xor", "asl", "dec", "inc", "lsr",
    // 10 - 1f
    "rol", "ror", ".if", "whl", ".go", "bra", "bcc", "bcs",
    "beq", "bfl", "bge", "blt", "bmi", "bne", "bpl", "btr",
    // 20 - 2f
    "bvc", "bvs", "obj", "org", "phs", ".db", "pea", "per",
    "brl", ".md", "far", "fdr", "fzr", "inp", "lcl", "rls",
    // 30 - 3f
    "bit", "cpx", "cpy", "ldx", "ldy", "stx", "sty", "trb",
    "tsb", "stz", "pei", "rep", "sep", "jmp", "jsr", "jml",
    // 40 - 4f
    "jsl", "mvn", "mvp", "= ", "con", "epd", "epz", "eql",
    "equ", "set", ".da", "adr", "byt", "csp", "dby", "hby",
    // 50 - 5f
    "bby", "anx", "chn", "icl", "lib", "lnk", "msg", "psm",
    "rlb", "sbt", "ttl", "dci", "rvs", "str", "zro", "dfs",
    // 60 - 6f
    "hex", "usr", "sav", ".tf", "seg", "cpu", ".entry", ".ref",
    ".group", ".deref", "long", NULL, ".assume", NULL, NULL, NULL,
    // 70 - 7f
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, ".table",
    // 80 - 8f
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    // 90 - 9f
    ".el", ".fi", ".me", ".we", ".la", ".lx", ".sa", ".sx",
    "dph", "if1", "if2", "end", "exp", "gen", "lst", "nls",
    // a0 - af
    "nog", "nox", "pag", "pau", "nlc", "cnd", "asl", "lsr",
    "rol", "ror", "dec", "inc", "mvn", "mvp", "brk", "clc",
    // b0 - bf
    "cld", "cli", "clv", "dex", "dey", "inx", "iny", "nop",
    "pha", "php", "pla", "plp", "rti", "rts", "sec", "sed",
    // c0 - cf
    "sei", "tax", "tay", "tsx", "txa", "txs", "tya", "phx",
    "phy", "plx", "ply", "cop", "phb", "phd", "phk", "plb",
    // d0 - df
    "pld", "rtl", "stp", "swa", "tad", "tas", "tcd", "tcs",
    "tda", "tdc", "tsa", "tsc", "txy", "tyx", "wai", "xba",
    // e0 - ef
    "xce", ".proc", ".endp", ".table", ".endt", NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    // f0 - ff
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
};


/*
 * Parse a file.
 */
int ReformatLISA4::Process(const ReformatHolder* pHolder,
    ReformatHolder::ReformatID id, ReformatHolder::ReformatPart part,
    ReformatOutput* pOutput)
{
    const uint8_t* srcPtr = pHolder->GetSourceBuf(part);
    long srcLen = pHolder->GetSourceLen(part);
    int retval = -1;

    if (srcLen < kHeaderLen+2) {
        LOGI("  LISA4 too short");
        goto bail;
    }

    fUseRTF = false;

    uint16_t version;
    uint16_t symEnd;

    version = srcPtr[0x00] | srcPtr[0x01] << 8;
    symEnd = srcPtr[0x02] | srcPtr[0x03] << 8;
    fSymCount = srcPtr[0x04] | srcPtr[0x05] << 8;
    fOpTab = srcPtr[0x06];
    fAdTab = srcPtr[0x07];
    fComTab = srcPtr[0x08];
    fCpuType = srcPtr[0x09];

    LOGD("  LISA4 version = 0x%04x  symEnd=%d  symCount=%d",
        version, symEnd, fSymCount);
    LOGD("  LISA4  opTab=%d adTab=%d comTab=%d cpuType=%d",
        fOpTab, fAdTab, fComTab, fCpuType);

    if (symEnd > srcLen) {
        LOGI("  LISA4 bad symEnd");
        goto bail;
    }
    if (fSymCount > symEnd) {
        LOGI("  LISA4 funky symCount");
        goto bail;
    }
    if (fSymCount > 0) {
        fSymTab = new const uint8_t*[fSymCount];
        if (fSymTab == NULL)
            goto bail;
    }

    const uint8_t* symPtr;
    const uint8_t* endPtr;
    int symIdx;

    symPtr = srcPtr + kHeaderLen;
    endPtr = srcPtr + symEnd;
    if (symPtr > endPtr) {
        LOGI("  LISA4 GLITCH: bad symEnd");
        goto bail;
    }

    /*
     * Generate symbol table index.
     */
    symIdx = 0;
    while (symPtr < endPtr) {
        if (symIdx < fSymCount)
            fSymTab[symIdx++] = symPtr;
        while (*symPtr != '\0')
            symPtr++;
        symPtr++;
    }
    if (symIdx != fSymCount) {
        LOGI("  LISA4 err: symIdx is %d, symCount is %d", symIdx, fSymCount);
        goto bail;
    }

    LOGI("  LISA4 symPtr=0x%p endPtr=0x%p symIdx=%d", symPtr, endPtr, symIdx);

    /*
     * Process source lines.
     */
    const uint8_t* codePtr;
    int lineNum;

    codePtr = srcPtr + symEnd;
    endPtr = srcPtr + srcLen;
    assert(codePtr < endPtr);
    lineNum = 0;

    while (codePtr < endPtr) {
        uint8_t flagByte;
        int lineLen;

        lineNum++;
        OutputStart();

        flagByte = *codePtr++;
        if (flagByte < 0x80) {
            /* explicit length, complex line */
            lineLen = flagByte;
            /* subtract 1 from flagByte, because len includes flagByte */
            if (flagByte > 0)
                ProcessLine(codePtr, flagByte-1);
        } else {
            /* SpecMnem - locals, labels, comments */
            if (flagByte >= kLocalTKN) {
                lineLen = 1;
                if (flagByte == kComntSemiTKN) {
                    Output(';');
                } else if (flagByte == kCommentTKN) {
                    Output('*');
                } else if (flagByte == kBlankTKN) {
                    // just a blank line
                } else if (flagByte < kLabelTKN) {
                    /* 0xf0 - 0xf9 - local numeric labels, e.g. "^1" */
                    Output('^');
                    Output('0' + flagByte - 0xf0);
                } else if (flagByte < kMacroTKN) {
                    /* 0xfa - 0xfb */
                    if (flagByte == 0xfa) {
                        /* label */
                        lineLen = 3;
                        int tmp = *codePtr | *(codePtr+1) << 8;
                        PrintSymEntry(tmp);
                    } else {
                        /* not used?? */
                        assert(lineLen == 1);
                        Output("??? ");
                    }
                } else {
                    /* macro (only object on line) */
                    assert(flagByte == kMacroTKN);
                    OutputTab(fOpTab);
                    int idx;
                    idx = *codePtr | *(codePtr+1) << 8;
                    Output('_');        // MacroChar
                    PrintSymEntry(idx);
                    lineLen = 3;
                }
            } else {
                /* OutMnem - simple, standard mnemonic */
                lineLen = 1;
                OutputTab(fOpTab);
                if (gMnemonics4[flagByte])
                    Output(gMnemonics4[flagByte]);
                else
                    Output("!BAD MNEMONIC!");
            }
        }
        

        if (lineLen == 0) {
            /* end of file */
            break;
        }

        OutputFinish();
        //BufPrintf("%d: %s\r\n", lineNum, GetOutBuf());
        BufPrintf("%s\r\n", GetOutBuf());

        codePtr += lineLen-1;
    }

    LOGI("  LISA4 codePtr=0x%p endPtr=0x%p numLines=%d",
        codePtr, endPtr, lineNum-1);
    LOGI("  LISA4 extra = %d", endPtr - codePtr);

    SetResultBuffer(pOutput);
    retval = 0;

bail:
    delete[] fSymTab;
    fSymTab = NULL;
    return retval;
}

void ReformatLISA4::ProcessLine(const uint8_t* codePtr, int len)
{
    uint8_t mnemonic = 0;

    if (*codePtr == kComntSemiTKN || *codePtr == kComntStarTKN ||
        *codePtr == kErrlnTKN)
    {
        switch (*codePtr) {
        case kComntSemiTKN:     Output(';');    break;
        case kComntStarTKN:     Output('*');    break;
        case kErrlnTKN:         Output('!');    break;
        default:
            assert(false);
        }
        codePtr++;
        while (--len)
            Output(*codePtr++ & 0x7f);

        goto bail;
    } else if (*codePtr == kMacroTKN) {
        /* handle macro */
        int idx;
        idx = *++codePtr;
        idx |= *++codePtr << 8;
        OutputTab(fOpTab);
        Output('_');        // MacroChar
        PrintSymEntry(idx);
        codePtr++;
        len -= 3;
        mnemonic = kMacroTKN;
        goto ConvtOperand;
    } else if (*codePtr == kLabelTKN) {
        /* handle label at start of line */
        uint16_t idx;
        idx = *++codePtr;
        idx |= *++codePtr << 8;
        PrintSymEntry(idx);
        codePtr++;
        len -= 3;
        // goto ConvtMnem
    } else if (*codePtr >= kLocalTKN) {
        /* handle local label (^) */
        Output('^');
        Output((char) (*codePtr - 0xc0));
        codePtr++;
        len--;
        // goto ConvtMnem
    } else {
        /* no label; current value is the mnemonic; continue w/o advancing */
        // fall through to ConvtMnem
    }

    /* ConvtMnem */
    mnemonic = *codePtr++;
    len--;
    if (mnemonic >= kMacroTKN) {
        /* OutMacro */
        assert(mnemonic == kMacroTKN);
        OutputTab(fOpTab);
        int idx;
        idx = *codePtr++;
        idx |= *codePtr++ << 8;
        Output('_');        // MacroChar
        PrintSymEntry(idx);
        len -= 2;
        //printf("{MAC:%d}", len);
    } else {
        OutputTab(fOpTab);
        if (gMnemonics4[mnemonic] != NULL)
            Output(gMnemonics4[mnemonic]);
        else {
            Output("!BAD MNEMONIC!");
            LOGI("  LISA4 bad mnemonic 0x%02x", mnemonic);
            DebugBreak();
        }
        if (mnemonic >= kSS) {
            /* CnvMnem2 - mnemonic has no associated operand */
            /* need to fall into ConvertOperand to show comment */
            if (len > 0) {
                /* can only be comment here; skip comment token */
                if (*codePtr != kComntSemiTKN)
                    printf("{SKIP=0x%02x,len=%d}", *codePtr, len);
                codePtr++;
                len--;
            }
        }
    }

ConvtOperand:
    /* ConvtOperand */
    //printf("{cen=%d}", len);
    ConvertOperand(mnemonic, &codePtr, &len);

bail:
    if (len > 0)
        printf("{LEN=%d}", len);

    return;
}

/*
 * ConvtOperand
 */
void ReformatLISA4::ConvertOperand(uint8_t mnemonic,
    const uint8_t** pCodePtr, int* pLen)
{
    /*
     * Address header char.
     */
    static const char kAdrsModeHeader[] = {
        0, 0, 0, 0, 0, 0, 0, 0, 0,      // 0-8 are null
        '(', '(', '(', '(',             // 9-12
        '[', '[',                       // 13-14
        0                               // 15
    };

    /*
     * operand lookup table - 1st char
     *     0 : not simple operand
     *  b7=1 : 1st char of simple operand
     */
    static const char kOperandTbl1[] =
        "+-*/&|^="          // 0-7
        "<>%<><~-"          // 8-F
        "01234567"          //10-17
        "89\0\0\0\0\0\0"    //18-1F
        "++++++++"          //20-27
        "++\0\0\0\0\0\0"    //28-2F
        "\0<>\0\0\0\0\0"    //30-37
        "\0*@#/^|\\"        //38-3F
        "<<<<<<<<"          //40-47
        "<<??????"          //48-4F
        ">>>>>>>>"          //50-57
        ">>????\0?"         //58-5F
        "\0\0\0\0\0\0\0\0"  //60-67
        "\0\0\0\0\0\0\0\0"  //68-6f
        "\0\0\0\0\0\0\0\0"  //70-77
        "\0\0\0\0\0\0\0\0"  //78-7f
    ;

    /*
     * operand lookup table - 2nd char
     *     0 : only 1 char
     *     1 : was unary op
     *  b7=1 : 2nd char of simple operand
     *
     * (Changed numeric 1 to '!'.  Bit 7 never set.  Normally it's set
     * for anything that isn't numeric 0 or 1.)
     */
    static const char kOperandTbl2[] =
        "\0\0\0\0\0\0\0\0"  // 0-7
        "\0\0\0==>!!"       // 8-F      note: 1's mark unaries
        "\0\0\0\0\0\0\0\0"  //10-17
        "\0\0\0\0\0\0\0\0"  //18-1F
        "01234567"          //20-27
        "89\0\0\0\0\0\0"    //28-2F
        "\0<>\0\0\0\0\0"    //30-37
        "\0\0!!!!!!"        //38-3F     note: 1's mark unaries
        "01234567"          //40-47
        "89012345"          //48-4F
        "01234567"          //50-57
        "896789\0#"         //58-5F
        "\0\0\0\0\0\0\0\0"  //60-67
        "\0\0\0\0\0\0\0\0"  //68-6f
        "\0\0\0\0\0\0\0\0"  //70-77
        "\0\0\0\0\0\0\0\0"  //78-7f
    ;

    /*
     * operator lookup table
     *     0 : not operator
     *     1 : complex operator      
     *  b7=1 : 1st char of simple operator
     *
     * (Changed numeric 1 to '!'.  Bit 7 never set.)
     */
    static const char kOperatorTbl1[] =
        "+-*/&|^="          // 0-7
        "<>%<><\0\0"        // 8-F
        "\0\0\0\0\0\0\0\0"  //10-17
        "\0\0\0\0\0\0\0\0"  //18-1F
        "!!!!!!!!"          //20-27
        "!!!!!!!!"          //28-2F
        "\0<>\0\0\0\0\0"    //30-37
        "\0\0\0\0\0\0\0\0"  //38-3F
        "\0\0\0\0\0\0\0\0"  //40-47
        "\0\0\0\0\0\0\0\0"  //48-4F
        "\0\0\0\0\0\0\0\0"  //50-57
        "\0\0\0\0\0\0\0\0"  //58-5F
        "\0\0\0\0\0\0\0\0"  //60-67
        "\0\0\0\0\0\0\0\0"  //68-6f
        "\0\0\0\0\0\0\0\0"  //70-77
        "\0\0\0\0\0\0\0\0"  //78-7f
    ;
    static const char* kOperatorTbl2 = kOperandTbl2;

    static const char* kAdrsModeTrailer[] = {
        NULL, NULL, NULL, ",X",
        ",X", ",X", NULL, ",S",
        ",Y", "),Y", ",X)", ")",
        ",S),Y", "]", "],Y", NULL,
    };


    const uint8_t* codePtr = *pCodePtr;
    int len = *pLen;
    OperandResult result;
    uint8_t adrsMode = 0;
    uint8_t val;
    char ch;

    if (mnemonic == kMacroTKN || mnemonic < kSS) {
        /* ConvtOperand */
        OutputTab(fAdTab);
        if (mnemonic != kMacroTKN) {
            if (mnemonic < kGROUP3_tkns ||
                !(mnemonic < kGROUP5_tkns || mnemonic == kMVN_tkn ||
                  mnemonic == kMVP_tkn || mnemonic >= kGROUP6_tkns))
            {
                if (len <= 0) {
                    Output("!BAD ADRS!");
                } else {
                    adrsMode = *codePtr++;
                    len--;
                }
            }
            if (adrsMode < NELEM(kAdrsModeHeader)) {
                ch = kAdrsModeHeader[adrsMode];
                if (ch != 0)
                    Output(ch);
            } else {
                Output("!BAD ADRSMODE!");
            }
        }
        //printf("{ven=%d val=0x%02x}", len, *codePtr);

        /* OutOprnd */
        while (len > 0) {
            bool doOutOprtr = false;
            val = *codePtr++;
            len--;

            if (val >= 0x80) {
                if (val == kLabelTKN) {
                    /* OutLabel */
                    int idx;
                    idx = *codePtr++;
                    idx |= *codePtr++ << 8;
                    len -= 2;
                    PrintSymEntry(idx);
                    doOutOprtr = true;
                } else if (val == kComntSemiTKN) {
                    break;      // out of while, to OutOprndDone */
                } else {
                    /* illegal token */
                    Output('!');
                    Output(',');
                    /* keep looping in OutOprnd */
                }
            } else {
                /* OutOpr2 */
                ch = kOperandTbl1[val];
                if (ch != '\0') {
                    /* simple operand */
                    Output(ch);
                    ch = kOperandTbl2[val];
                    if (ch == '!')
                        continue;       // unary, no operator, go to OutOprnd
                    else if (ch != '\0')
                        Output(ch);
                    doOutOprtr = true;
                } else {
                    /* OutOprComp - complex operand */
                    result = PrintComplexOperand(val, &codePtr, &len);
                    if (result == kResultGotoOutOprtr)
                        goto OutOprtr;
                    // else continue around in OutOprnd
                }
            }

            if (doOutOprtr) {
OutOprtr:
                uint8_t opr;

                if (!len)
                    break;
                opr = *codePtr++;
                len--;

                if (opr >= 0x80) {
not_operator:
                    if (opr == kComntSemiTKN)
                        break;  // goto OutOprndDone
                    else {
                        /* must be two sequential operands */
                        Output(',');
                        codePtr--;  // back up
                        len++;
                        // continue around to OutOprnd
                    }
                } else {
                    char opch;

                    opch = kOperatorTbl1[opr];
                    if (opch == 0) {
                        goto not_operator;
                    } else if (opch == 0 || opch == '!') {
                        /* complex */
                        Output('+');
                        opch = kOperatorTbl2[opr];
                        //printf("{opch=0x%02x}", opch);
                        if (opch != '\0') {
                            Output(opch);
                            goto OutOprtr;      // look for another
                        } else {
                            int num;
                            num = opr - 0x10;
                            result = PrintNum(num, &codePtr, &len);
                            if (result == kResultGotoOutOprtr)
                                goto OutOprtr;
                        }
                    } else {
                        /* simple */
                        Output(' ');
                        Output(opch);
                        opch = kOperatorTbl2[opr];
                        if (opch != '\0')
                            Output(opch);
                        Output(' ');
                        // continue to OutOprnd
                    }
                }
            }
        }
    }

    /* OutOprndDone */
    if (adrsMode != 0) {
        if (adrsMode < NELEM(kAdrsModeHeader)) {
            if (kAdrsModeTrailer[adrsMode] != NULL)
                Output(kAdrsModeTrailer[adrsMode]);
        } else {
            Output("!BAD ADRSMODE!");
            printf("{ADRS=%d}", adrsMode);
        }
    }

    if (len > 0) {
        OutputTab(fComTab);
        Output(';');
        while (len--)
            Output(*codePtr++ & 0x7f);
    }

//bail:
    *pCodePtr = codePtr;
    *pLen = len;
}

/*
 * CnvrtDec - convert to decimal output.
 */
void ReformatLISA4::PrintDec(int count, const uint8_t** pCodePtr,
    int* pLen)
{
    const uint8_t* codePtr = *pCodePtr;
    int len = *pLen;
    long val = 0;
    char buf[12];       // 4 bytes, max 10 chars + sign + nul

    for (int i = 0; i < count; i++) {
        val |= *codePtr++ << (8 * i);
        len--;
    }
    sprintf(buf, "%lu", val);
    Output(buf);

    *pCodePtr = codePtr;
    *pLen = len;
}

/*
 * CnvrtHex - convert to hex output.
 */
void ReformatLISA4::PrintHex(int count, const uint8_t** pCodePtr,
    int* pLen)
{
    const uint8_t* codePtr = *pCodePtr;
    int len = *pLen;
    uint8_t val;

    Output('$');
    for (int i = count-1; i >= 0; i--) {
        val = *(codePtr+i);
        Output(gHexDigit[(val & 0xf0) >> 4]);
        Output(gHexDigit[val & 0x0f]);
    }
    codePtr += count;
    len -= count;

    *pCodePtr = codePtr;
    *pLen = len;
}

/*
 * CnvrtBin - convert to binary output.
 */
void ReformatLISA4::PrintBin(int count, const uint8_t** pCodePtr,
    int* pLen)
{
    const uint8_t* codePtr = *pCodePtr;
    int len = *pLen;
    uint8_t val;
    char buf[9];

    buf[8] = '\0';

    Output('%');
    for (int i = count-1; i >= 0; i--) {
        val = *(codePtr+i);
        for (int bit = 0; bit < 8; bit++)
            buf[bit] = '0' + ((val >> (7-bit)) & 0x01);
        Output(buf);
    }

    codePtr += count;
    len -= count;

    *pCodePtr = codePtr;
    *pLen = len;
}

/*
 * OUTNUM
 */
ReformatLISA4::OperandResult ReformatLISA4::PrintNum(uint8_t opr,
    const uint8_t** pCodePtr, int* pLen)
{
    OperandResult result = kResultUnknown;
    const uint8_t* codePtr = *pCodePtr;
    int len = *pLen;
    int idx;

    switch (opr) {
    case kDec3_tkn:
        PrintDec(3, &codePtr, &len);
        break;
    case kDec2_tkn:
        PrintDec(2, &codePtr, &len);
        break;
    case kDec1_tkn:
        PrintDec(1, &codePtr, &len);
        break;
    case kHex3_tkn:
        PrintHex(3, &codePtr, &len);
        break;
    case kHex2_tkn:
        PrintHex(2, &codePtr, &len);
        break;
    case kHex1_tkn:
        PrintHex(1, &codePtr, &len);
        break;
    case kBin3_tkn:
        PrintBin(3, &codePtr, &len);
        break;
    case kBin2_tkn:
        PrintBin(2, &codePtr, &len);
        break;
    case kBin1_tkn:
        PrintBin(1, &codePtr, &len);
        break;
    case kcABS_tkn:
        /* coerce absolute */
        if (*codePtr == kLabelTKN) {
            codePtr++;
            len--;
        }
        idx = *codePtr++;
        idx |= *codePtr++ << 8;
        len -= 2;
        PrintSymEntry(idx);
        Output(':');
        Output('A');
        break;
    case kcLONG_tkn:
        /* coerce long */
        if (*codePtr == kLabelTKN) {
            codePtr++;
            len--;
        }
        idx = *codePtr++;
        idx |= *codePtr++ << 8;
        len -= 2;
        PrintSymEntry(idx);
        Output(':');
        Output('L');
        break;
    case kMacE_tkn:
        /* macro expression */
        Output('?');
        Output(':');
        result = kResultGotoOutOprnd;
        break;
    default:
        if (opr >= kStr31_tkn+1) {
            /* CheckMoreOprnd - none currently */
            // (not expected, but not much we can do)
            Output("{CheckMoreOprnd}");
        } else {
            /* CheckStrings */
            uint8_t strLen;
            uint8_t val;
            uint8_t delimit;

            if ((opr & 0x1f) == 0) {
                strLen = *codePtr++;
                len--;
            } else {
                strLen = opr & 0x1f;
            }
            if (strLen > len) {
                Output("!BAD STR!");
                printf("{opr=0x%02x, strLen=%d, len=%d}", opr, strLen, len);
                return kResultFailed;
            }
            val = *codePtr;
            if (val < 0x80) {
                /* ISAPOST */
                delimit = '\'';
            } else {
                /* DETKNSTR */
                delimit = '\"';
            }
            Output(delimit);
            while (strLen--) {
                val = *codePtr++ & 0x7f;
                len--;

                Output(val);
                if (val == delimit)
                    Output(val);
            }
            Output(delimit);
        }
        break;
    }

    if (result == kResultUnknown)
        result = kResultGotoOutOprtr;

    *pCodePtr = codePtr;
    *pLen = len;
    return result;
}

/*
 * OutOprComp
 */
ReformatLISA4::OperandResult ReformatLISA4::PrintComplexOperand(uint8_t opr,
    const uint8_t** pCodePtr, int* pLen)
{
    if (opr != kBign_tkn)
        return PrintNum(opr, pCodePtr, pLen);

/*
    const uint8_t* codePtr = *pCodePtr;
    int len = *pLen;
    *pCodePtr = codePtr;
    *pLen = len;
*/

    uint8_t subClass;

    /* OutOprComp */
    subClass = *(*pCodePtr)++;
    (*pLen)--;
    if (subClass == kBigndec4_tkn) {
        PrintDec(4, pCodePtr, pLen);
    } else if (subClass == kBignhex4_tkn) {
        PrintHex(4, pCodePtr, pLen);
    } else if (subClass == kBignbin4_tkn) {
        PrintBin(4, pCodePtr, pLen);
    } else if (subClass == kBignhexs_tkn) {
        /* hex string, for HEX pseudo-op */
        uint8_t hexLen = *(*pCodePtr)++;
        (*pLen)--;
        if (hexLen > *pLen) {
            Output("!BAD HEX!");
            return kResultFailed;
        }
        while (hexLen--) {
            uint8_t val = *(*pCodePtr)++;
            (*pLen)--;
            Output(gHexDigit[(val & 0xf0) >> 4]);
            Output(gHexDigit[val & 0x0f]);
        }
    } else if (subClass == kBignstring_tkn) {
        /* undelimited string */
        uint8_t strLen = *(*pCodePtr)++;
        (*pLen)--;
        if (strLen > *pLen) {
            Output("!BAD USTR!");
            return kResultFailed;
        }
        while (strLen--) {
            uint8_t val = *(*pCodePtr)++;
            (*pLen)--;
            Output(val & 0x7f);
        }
    } else {
        Output("!BAD CPLX OPRND!");
        DebugBreak();
        printf("OPR=%d SUBCLASS=%d", opr, subClass);
        return kResultFailed;
    }

    return kResultGotoOutOprtr;
}

/*
 * Print symbol table entry.
 */
void ReformatLISA4::PrintSymEntry(int ent)
{
    if (ent < 0 || ent >= fSymCount) {
        Output("!BAD SYM!");
        return;
    }

    const uint8_t* str = fSymTab[ent];

    uint8_t uc;
    str++;
    while (1) {
        uc = *str++;
        if (!uc)
            break;
        else if (uc < 0x80)
            uc |= 0x20;
        Output(uc & 0x7f);
    }
}
