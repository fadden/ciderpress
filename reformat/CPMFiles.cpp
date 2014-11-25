/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Special handling for files on CP/M disks.
 */
#include "StdAfx.h"
#include "CPMFiles.h"


const int kCtrlZ = 0x1a;        // end-of-file indicator

/*
 * Table determining what's a binary character and what isn't.  This is
 * roughly the same table as is used in GenericArchive.cpp.  The code will
 * additionally allow Ctrl-Z, and will allow occurrences of 0x00 that appear
 * after the Ctrl-Z.
 *
 * Even if we don't allow high ASCII, we must still allow 0xe5 if it occurs
 * after a Ctrl-Z.
 *
 * After looking at the generic ISO-latin-1 table, Paul Schlyter writes: 
 * -----
 * Remove 88, 89, 8A, 8C and 8D as well from this table.  The CP/M version of  
 * Wordstar uses the hi bit of any character for its own uses - for instance
 * 0D 0A is a "soft end-of-line" which Wordstar can move around, while 8D 8A is 
 * a "hard end-of-line" which WordStar does not move around.  Other characters
 * can have this bit used to signal hilighted text.  On a lot of CP/M systems
 * the hi bit is ignored when displaying characters (= sending the characters to
 * the standard console output), thus one can often "type" a WordStar file and
 * have it displayed as readable text.
 * -----
 */
static const char gIsBinary[256] = {
    1, 1, 1, 1, 1, 1, 1, 1,  0, 0, 0, 1, 0, 0, 1, 1,    /* ^@-^O */
    1, 1, 1, 1, 1, 1, 1, 1,  1, 1, 1, 1, 1, 1, 1, 1,    /* ^P-^_ */
    0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0,    /*   - / */
    0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0,    /* 0 - ? */
    0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0,    /* @ - O */
    0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0,    /* P - _ */
    0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0,    /* ` - o */
    0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 1,    /* p - DEL */
    1, 1, 1, 1, 1, 1, 1, 1,  0, 0, 0, 1, 0, 0, 1, 1,    /* 0x80 */
    1, 1, 1, 1, 1, 1, 1, 1,  1, 1, 1, 1, 1, 1, 1, 1,    /* 0x90 */
    0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0,    /* 0xa0 */
    0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0,    /* 0xb0 */
    0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0,    /* 0xc0 */
    0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0,    /* 0xd0 */
    0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0,    /* 0xe0 */
    0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0,    /* 0xf0 */
};

/*
 * Decide whether or not this is a CP/M text file.
 *
 * End-of-file is at the first Ctrl-Z, but we can't stop there because it
 * could be a binary file with a leading Ctrl-Z (e.g. PNG).
 */
void ReformatCPMText::Examine(ReformatHolder* pHolder)
{
    ReformatHolder::ReformatApplies applies = ReformatHolder::kApplicNot;
    const uint8_t* ptr = pHolder->GetSourceBuf(ReformatHolder::kPartData);
    long fileLen = pHolder->GetSourceLen(ReformatHolder::kPartData);
    const char* nameExt = pHolder->GetNameExt();
    bool foundCtrlZ = false;

    /* only show this on CP/M disks */
    if (pHolder->GetSourceFormat() != ReformatHolder::kSourceFormatCPM)
        goto done;
    applies = ReformatHolder::kApplicProbablyNot;

    /* allow, but don't default to, text conversion of ".com" files */
    if (stricmp(nameExt, ".com") == 0) {
        LOGI("Not reformatting '.com' file as text");
        goto done;
    }

    /*
     * Scan file, looking for illegal chars.
     *
     * Thought for the day: could also require that Ctrl-Z appear in the
     * last 128 bytes of the file.  May want to count all high-ASCII values
     * as illegal but allow a certain percentage of "illegal" characters in
     * the mix.
     */
    while (fileLen--) {
        if (*ptr == kCtrlZ) {
            foundCtrlZ = true;
        } else if (foundCtrlZ && *ptr == 0x00) {
            /* do nothing -- 0x00 is okay if it comes after Ctrl-Z */
        } else {
            if (gIsBinary[*ptr]) {
                LOGI("CP/M found binary char 0x%02x at offset 0x%04x",
                    *ptr,
                    ptr - pHolder->GetSourceBuf(ReformatHolder::kPartData));
                break;
            }
        }
        ptr++;
    }
    if (fileLen == -1)
        applies = ReformatHolder::kApplicProbably;

done:
    pHolder->SetApplic(ReformatHolder::kReformatCPMText, applies,
        ReformatHolder::kApplicNot, ReformatHolder::kApplicNot);
}

/*
 * Convert EOL markers.
 *
 * The primary difference between "CP/M text" and other formats is that we
 * stop on the first occurrence of Ctrl-Z.
 *
 * Generally speaking, CP/M text files should already be in CRLF format, so
 * this will go quickly.
 */
int ReformatCPMText::Process(const ReformatHolder* pHolder,
    ReformatHolder::ReformatID id, ReformatHolder::ReformatPart part,
    ReformatOutput* pOutput)
{
    const uint8_t* srcBuf = pHolder->GetSourceBuf(part);
    long srcLen = pHolder->GetSourceLen(part);
    fUseRTF = false;

    if (pHolder->GetSourceLen(part) == 0)
        return -1;

    for (long ll = 0; ll < srcLen; ll++) {
        if (*srcBuf == kCtrlZ /*|| *srcBuf == '\0'*/) {
            srcLen = ll;
            break;
        }
        srcBuf++;
    }

    ConvertEOL(pHolder->GetSourceBuf(part), srcLen, true);

    SetResultBuffer(pOutput);
    return 0;
}
