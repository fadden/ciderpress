/*
 * CiderPress
 * Copyright (C) 2009 by CiderPress authors.  All Rights Reserved.
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Convert 8-bit word processor files.
 *
 * Most formats convert reasonably well with "Converted Text", but this
 * allows the files to be handled more transparently (e.g. Magic Window
 * "formatted files", which can be mistaken for code.
 */
#include "StdAfx.h"
#include "Text8.h"


/*
 * ===========================================================================
 *      Magic Window / Magic Window II
 * ===========================================================================
 */

/*
 * Magic Window and Magic Window II appear to use the same format for their
 * "formatted files".  The files are of type 'B', with a valid address field,
 * and what looks like junk in the length field.  The files have a 256-byte
 * header that seems to hold some sort of title string as well as some
 * binary goodies that I'm not sure what they are.
 *
 * The data from offset 256 on is entirely mixed-case high-ASCII text.  It
 * may contain printer-specific escape codes for bold, italic, etc.
 *
 * A ".MW" filename suffix is enforced by the program.
 */

/*
 * Decide whether or not we want to handle this file.
 */
void ReformatMagicWindow::Examine(ReformatHolder* pHolder)
{
    if (pHolder->GetFileType() == kTypeBIN) {
        bool isMW = ReformatMagicWindow::IsFormatted(pHolder);
        bool isDotMW = stricmp(pHolder->GetNameExt(), ".MW") == 0;

        if (isMW && isDotMW) {
            /* gotta be */
            pHolder->SetApplic(ReformatHolder::kReformatMagicWindow,
                ReformatHolder::kApplicYes,
                ReformatHolder::kApplicNot, ReformatHolder::kApplicNot);
        } else if (isDotMW) {
            /* right type and name; maybe our test is broken? */
            pHolder->SetApplic(ReformatHolder::kReformatMagicWindow,
                ReformatHolder::kApplicProbably,
                ReformatHolder::kApplicNot, ReformatHolder::kApplicNot);
        } else if (isMW) {
            /* not likely, but offer it as non-default option */
            pHolder->SetApplic(ReformatHolder::kReformatMagicWindow,
                ReformatHolder::kApplicProbablyNot,
                ReformatHolder::kApplicNot, ReformatHolder::kApplicNot);
        } else {
            /* not one of ours */
            pHolder->SetApplic(ReformatHolder::kReformatMagicWindow,
                ReformatHolder::kApplicNot,
                ReformatHolder::kApplicNot, ReformatHolder::kApplicNot);
        }
    } else {
        /* "unformatted" text even if ".MW"; nothing special required */
        pHolder->SetApplic(ReformatHolder::kReformatMagicWindow,
            ReformatHolder::kApplicNot,
            ReformatHolder::kApplicNot, ReformatHolder::kApplicNot);
    }
}

/*
 * Figure out if this is a Magic Window "formatted" file.
 *
 * I don't know much about the format, so this is based on the similarities
 * observed between half a dozen documents from different sources.
 */
/*static*/ bool ReformatMagicWindow::IsFormatted(const ReformatHolder* pHolder)
{
    const uint8_t* ptr = pHolder->GetSourceBuf(ReformatHolder::kPartData);
    long srcLen = pHolder->GetSourceLen(ReformatHolder::kPartData);
    int i, count00, count20;


    /* want 256-byte header, plus a few bytes to check text */
    if (srcLen < kHeaderLen+8)
        return false;

    /*
     * First byte always seems to be 0x8d.
     */
    if (ptr[0x00] != 0x8d)
        return false;

    /*
     * 0x58 - 0xa0 is mostly filled with 0x00 (for Magic Window) or 0x20
     * (for Magic Window II).  Both seem to have space for the title in the
     * preceeding part, but it's high-ASCII for MW and low-ASCII for MW2.
     *
     * Expect 50 out of 72 to match.  If this is actually just uninitialized
     * data then this test will be bogus.
     */
    count00 = count20 = 0;
    for (i = 0x58; i < 0xa0; i++) {
        if (ptr[i] == 0x00)
            count00++;
        if (ptr[i] == 0x20)
            count20++;
    }
    if (count00 < 50 && count20 < 50)
        return false;

    /*
     * 0xa2 has some recognizeable bytes; sample values:
     *  MW  42 06 36 50 08 40
     *  MW2 42 06 36 55 08 40
     *  MW2 42 04 3a 50 00 50
     * Not really sure what to make of these.  If we can bracket these
     * values we might have something.
     */
    if (ptr[0xa2] != 0x42 ||
        (ptr[0xa3] < 2 && ptr[0xa3] > 10) ||
        (ptr[0xa4] < 0x30 && ptr[0xa4] > 0x40))
        return false;

    /*
     * Make sure the rest of the file is 100% high ASCII.
     */
    ptr += kHeaderLen;
    srcLen -= kHeaderLen;
    while (srcLen--) {
        if ((*ptr & 0x80) == 0)
            return false;
    }

    return true;
}


/*
 * Skip the header and text-convert the rest.
 */
int ReformatMagicWindow::Process(const ReformatHolder* pHolder,
    ReformatHolder::ReformatID id, ReformatHolder::ReformatPart part,
    ReformatOutput* pOutput)
{
    const uint8_t* srcPtr = pHolder->GetSourceBuf(part);
    long srcLen = pHolder->GetSourceLen(part);
    long length = srcLen;
    int retval = -1;

    fUseRTF = false;

    RTFBegin();

    if (srcLen <= kHeaderLen)
        goto bail;

    ConvertEOL(srcPtr + kHeaderLen, srcLen - kHeaderLen, true);

    //done:
    RTFEnd();

    SetResultBuffer(pOutput);
    retval = 0;

bail:
    return retval;
}


/*
 * ===========================================================================
 *      Gutenberg Word Processor
 * ===========================================================================
 */

/*
 * Decide whether or not we want to handle this file.
 */
void ReformatGutenberg::Examine(ReformatHolder* pHolder)
{
    if ((pHolder->GetFileType() == kTypeTXT) && 
        (pHolder->GetSourceFormat() == ReformatHolder::kSourceFormatGutenberg)) {

        pHolder->SetApplic(ReformatHolder::kReformatGutenberg,
            ReformatHolder::kApplicYes,
            ReformatHolder::kApplicNot, ReformatHolder::kApplicNot);
    }
}

/*
 * Convert the text.
 */
int ReformatGutenberg::Process(const ReformatHolder* pHolder,
    ReformatHolder::ReformatID id, ReformatHolder::ReformatPart part,
    ReformatOutput* pOutput)
{
    const uint8_t* srcPtr = pHolder->GetSourceBuf(part);
    long srcLen = pHolder->GetSourceLen(part);
    long length = srcLen;
    int retval = -1;

    fUseRTF = false;

    RTFBegin();

    ConvertEOL(srcPtr, srcLen, true, true);

    RTFEnd();

    SetResultBuffer(pOutput);
    retval = 0;

    return retval;
}
